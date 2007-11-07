#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <lauxlib.h>

//#define DEBUG 1
#include "version.h"
#include "macros.h"
#include "elua.h"

/* $Id$ */

// Note:
// XXX Includes, see: http://pgl.yoyo.org/luai/i/dofile

#define CTX_TEXT 1
#define CTX_EVAL 2
#define CTX_COMMENT 4
#define CTX_PRINT 8
#define CTX_OUTPUT_STARTED 16
#define CTX_MULTILINE 32

#ifdef DEBUG
#define log_parse(fmt, ...) fprintf(stdout, "%lu:%-2lu  " fmt "\n", line, column, ##__VA_ARGS__)
#else
#define log_parse(fmt, ...)
#endif

/* ------------------------------------------------------------------------- */

static int set_file_error(lua_State *L/*, const char *what, const char *filename*/) {
  const char *serr = strerror(errno);
  //lua_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  lua_pushfstring(L, "IO Error: %s", serr);
  return LUA_ERRFILE;
}

const char *elua_load_cstr_reader(lua_State *L, void *ud, size_t *size) {
  cstr_t *buf = (cstr_t *)ud;
  (void)L;
  if(buf->length == 0) return NULL;
  *size = buf->length;
  buf->length = 0;
  //log_error("reader sending (%lu) '%s'", *size, buf->ptr);
  return (const char *)buf->ptr;
}

/*typedef struct {
  char    *prefix_ptr;
  size_t  prefix_len;
  cstr_t  *buf;
} elua_prefixed_cstr_t;

const char *elua_load_prefixed_cstr_reader(lua_State *L, void *ud, size_t *size) {
  elua_prefixed_cstr_t *pc = (elua_prefixed_cstr_t *)ud;
  (void)L;
  if(pc->prefix_len) {
    *size = pc->prefix_len;
    pc->prefix_len = 0;
    return pc->prefix_ptr;
  }
  if(pc->buf->length == 0) return NULL;
  *size = pc->buf->length;
  pc->buf->length = 0;
  //log_error("reader sending (%lu) '%s'", *size, pc->buf->ptr);
  return (const char *)pc->buf->ptr;
}*/

/* ------------------------------------------------------------------------- */
/* Private */

/**
 * Push something onto the lua stack
 */
static int elua_push_buf(lua_State *L, const int context, int *output_started, elua_context_t *ctx) {
  
  int status = 0;
  
  if(ctx->buf->length == 0) {
    return 0;
  }
  
  if( ctx->conf->auto_begin_response_call && (!*output_started) && ((context & CTX_PRINT) || (!(context & CTX_EVAL))) ) {
    cstr_append(ctx->out, "send_headers()\n", 15);
    *output_started = 1;
  }
  
  if(context & CTX_EVAL) {
    if(context & CTX_COMMENT) {
      if(!ctx->conf->strip_comments) {
        log_debug("Push: comment: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
        if(context & CTX_MULTILINE) {
          cstr_append(ctx->out, "--[[", 4);
          cstr_append_cstr(ctx->out, ctx->buf);
          cstr_append(ctx->out, "]] ", 3); // was: "]]\n"
        }
        else {
          cstr_appendc(ctx->out, '-');
          cstr_appendc(ctx->out, '-');
          cstr_append_cstr(ctx->out, ctx->buf);
          cstr_appendc(ctx->out, ' '); // was: \n
        }
      }
    }
    else if(context & CTX_PRINT) {
      log_debug("Push: print: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
      
      cstr_append(ctx->out, "print(", 6);
      cstr_append_cstr(ctx->out, ctx->buf);
      cstr_appendc(ctx->out, ')');
      cstr_appendc(ctx->out, ' '); // was: \n
    }
    else {
      log_debug("Push: eval: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
      cstr_append_cstr(ctx->out, ctx->buf);
      cstr_appendc(ctx->out, ' '); // was: \n
    }
  }
  else {
    log_debug("Push: text: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
    
    cstr_append(ctx->out, "print(\"", 7);
    cstr_append_cstr(ctx->out, ctx->buf);
    cstr_appendc(ctx->out, '"');
    cstr_appendc(ctx->out, ')');
    cstr_appendc(ctx->out, ' '); // was: \n
  }
  
  cstr_reset(ctx->buf);
  
  return status;
}

/* ------------------------------------------------------------------------- */
/* Public */

// Initialize a new context
void elua_init_context(elua_context_t *ctx) {
  ctx->conf = (elua_conf_t*)malloc(sizeof(elua_conf_t));
  ctx->buf = cstr_new(4096);
  ctx->out = cstr_new(4096);
  elua_reset_conf(ctx->conf);
}

// Free an initialized context
void elua_free_context(elua_context_t *ctx) {
  free(ctx->conf);
  cstr_free(ctx->buf); free(ctx->buf);
  cstr_free(ctx->out); free(ctx->out);
}

void elua_reset_conf(elua_conf_t *conf) {
  conf->strip_comments = 1;
  conf->auto_begin_response_call = 0;
}

int elua_parse_file(lua_State *L, FILE *f, elua_context_t *ctx) {
  
  log_debug("Entered elua_parse_file");
  
  int c;
  int prev_c = -1;
  int prev_prev_c = -1;
  int context = CTX_TEXT; // parser context
  int status = 0;
  int output_started = 0;
  size_t line = 1;
  size_t column = 0;
  
  // Make sure buf is reset
  cstr_reset(ctx->buf);
  cstr_reset(ctx->out);
  
  
  log_debug("Entering read-loop");
  while(++column) {
    c = fgetc(f);
    // Handle EOF
    if(c == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(f)) {
        log_error("I/O Error %d: %s", errno, strerror(errno));
        //clearerr(f);
        status = set_file_error(L);
      }
      else if(ctx->buf->length != 0) {
        elua_push_buf(L, context, &output_started, ctx);
      }
      break; 
    }
    
    // In text context?
    if(context & CTX_TEXT) {
      if(prev_c == '<' && c == '%') {
        log_parse("Switch: TEXT -> EVAL <%%");
        assert(ctx->buf->ptr[ctx->buf->length-1] == '<');
        cstr_popc(ctx->buf); // remove '<'
        log_parse("Push: TEXT: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
        elua_push_buf(L, context, &output_started, ctx);
        context = CTX_EVAL;
      }
      else {
        if(c == '"' || c == '\n') { // Escape " and \n in text
          cstr_appendc(ctx->buf, '\\');
        }
        cstr_appendc(ctx->buf, c);
      }
    }
    // In eval context?
    else if(context & CTX_EVAL) {
      if(prev_c == '%' && c == '>') {
        log_parse("Switch: EVAL -> TEXT %%>");
        assert(ctx->buf->ptr[ctx->buf->length-1] == '%');
        cstr_popc(ctx->buf); // remove '%'
#ifdef DEBUG
          if(context & CTX_COMMENT) {
            log_parse("Push: EVAL Comment: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
          } else if(context & CTX_PRINT) {
            log_parse("Push: EVAL print: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
          } else {
            log_parse("Push: EVAL: (%lu) '%s'", ctx->buf->length, ctx->buf->ptr);
          }
#endif
        elua_push_buf(L, context, &output_started, ctx);
        context = CTX_TEXT;
      }
      else if(prev_prev_c == '<' && prev_c == '%') {
        // the first char after "<%"
        if(c == '#') {
          context |= CTX_COMMENT;
          log_parse("Switch: EVAL -> COMMENT <%%#");
        }
        else if(c == '=') {
          context |= CTX_PRINT;
          log_parse("Switch: EVAL -> PRINT <%%=");
        }
        else {
          cstr_appendc(ctx->buf, c);
        }
      }
      else {
        cstr_appendc(ctx->buf, c);
      }
    }
    //else { log_debug("nomatch %d (%d, %d)", context & CTX_EVAL, context, CTX_EVAL); }
    if(c == '\n') {
      if(!(context & CTX_MULTILINE)) {
        context |= CTX_MULTILINE;
      }
      line++;
      column = 0;
    }
    
    prev_prev_c = prev_c;
    prev_c = c;
  }
  
  return status;
}


// LUA addition
int lua_load_cstr(lua_State *L, cstr_t *buf, const char *filename) {
  return lua_load(L, elua_load_cstr_reader, (void *)buf, filename);
}


int elua_loadfile(lua_State *L, const char *fn, elua_context_t *ctx) {
  FILE *fp;
  int status;
  char *a_fn;
  
  a_fn = NULL;
  
  // Open file for reading
  if((fp = fopen(fn, "r")) == NULL) {
    lua_pushfstring(L, "Failed to open %s for reading: [%d] %s", fn, errno, strerror(errno));
    fclose(fp);
    return LUA_ERRFILE;
  }
  
  // Parse & Load
  if( (status = elua_parse_file(L, fp, ctx)) == 0 ) {
    // Filename with @-prefix
    size_t fn_size = strlen(fn);
    a_fn = (char *)malloc(fn_size+2);
    a_fn[0] = '@';
    memcpy(a_fn+1, fn, fn_size);
    a_fn[fn_size+1] = 0;
    
    // Evaluate into the LUA stack
    status = lua_load_cstr(L, ctx->out, a_fn);
    
    free(a_fn);
  }
  
  fclose(fp);
  return status;
}
