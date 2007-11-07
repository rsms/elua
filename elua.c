#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lauxlib.h>

//#define DEBUG 1
#include "version.h"
#include "macros.h"
#include "elua.h"

/* $Id$ */

// Note:
// XXX Includes, see: http://pgl.yoyo.org/luai/i/dofile

elua_conf_t elua_conf_defaults() {
  elua_conf_t c;
  c.strip_comments = 1;
  c.auto_begin_response_call = 0;
  return c;
}

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

static int set_file_error(lua_State *L/*, const char *what, const char *filename*/) {
  const char *serr = strerror(errno);
  //lua_pushfstring(L, "cannot %s %s: %s", what, filename, serr);
  lua_pushfstring(L, "IO Error: %s", serr);
  return LUA_ERRFILE;
}

const char *elua_load_cstr_reader(lua_State *L, void *ud, size_t *size) {
  cstr *buf = (cstr *)ud;
  (void)L;
  if(buf->length == 0) return NULL;
  *size = buf->length;
  buf->length = 0;
  //log_error("reader sending (%lu) '%s'", *size, buf->ptr);
  return (const char *)buf->ptr;
}

typedef struct {
  char    *prefix_ptr;
  size_t  prefix_len;
  cstr    *buf;
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
}


/**
 * Push something onto the lua stack
 */
static int elua_push_buf(lua_State *L, const int context, int *output_started, cstr *buf, cstr *out, elua_conf_t *conf) {
  
  int status = 0;
  
  if(buf->length == 0) {
    return 0;
  }
  
  if( conf->auto_begin_response_call && (!*output_started) && ((context & CTX_PRINT) || (!(context & CTX_EVAL))) ) {
    cstr_append(out, "send_headers()\n", 15);
    *output_started = 1;
  }
  
  if(context & CTX_EVAL) {
    if(context & CTX_COMMENT) {
      if(!conf->strip_comments) {
        log_debug("Push: comment: (%lu) '%s'", buf->length, buf->ptr);
        if(context & CTX_MULTILINE) {
          cstr_append(out, "--[[", 4);
          cstr_append(out, buf->ptr, buf->length);
          cstr_append(out, "]]\n", 3);
        }
        else {
          cstr_appendc(out, '-');
          cstr_appendc(out, '-');
          cstr_append(out, buf->ptr, buf->length);
          cstr_appendc(out, '\n');
        }
      }
    }
    else if(context & CTX_PRINT) {
      log_debug("Push: print: (%lu) '%s'", buf->length, buf->ptr);
      
      cstr_append(out, "io.write(", 9);
      cstr_append(out, buf->ptr, buf->length);
      cstr_appendc(out, ')');
      cstr_appendc(out, '\n');
    }
    else {
      log_debug("Push: eval: (%lu) '%s'", buf->length, buf->ptr);
      cstr_append(out, buf->ptr, buf->length);
      cstr_appendc(out, '\n');
    }
  }
  else {
    log_debug("Push: text: (%lu) '%s'", buf->length, buf->ptr);
    
    cstr_append(out, "io.write(\"", 10);
    cstr_append(out, buf->ptr, buf->length);
    cstr_appendc(out, '"');
    cstr_appendc(out, ')');
    cstr_appendc(out, '\n');
  }
  
  cstr_reset(buf);
  
  return status;
}



int elua_parse_file(lua_State *L, FILE *f, cstr *buf, cstr *out, elua_conf_t *conf) {
  
  log_debug("Entered elua_parse_file");
  
  int c;
  int prev_c = -1;
  int prev_prev_c = -1;
  int context = CTX_TEXT;
  int status = 0;
  int output_started = 0;
  size_t line = 1;
  size_t column = 0;
  
  // Make sure buf is reset
  cstr_reset(buf);
  cstr_reset(out);
  
  
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
      else if(buf->length != 0) {
        elua_push_buf(L, context, &output_started, buf, out, conf);
      }
      break; 
    }
    
    // In text context?
    if(context & CTX_TEXT) {
      if(prev_c == '<' && c == '%') {
        log_parse("Switch: TEXT -> EVAL <%%");
        cstr_popc(buf); // remove '<'
        log_parse("Push: TEXT: (%lu) '%s'", buf->length, buf->ptr);
        elua_push_buf(L, context, &output_started, buf, out, conf);
        context = CTX_EVAL;
      }
      else {
        if(c == '"' || c == '\n') { // Escape " and \n in text
          cstr_appendc(buf, '\\');
        }
        cstr_appendc(buf, c);
      }
    }
    // In eval context?
    else if(context & CTX_EVAL) {
      if(prev_c == '%' && c == '>') {
        log_parse("Switch: EVAL -> TEXT %%>");
        cstr_popc(buf); // remove '%'
        //#ifdef DEBUG
          if(context & CTX_COMMENT) {
            log_parse("Push: EVAL Comment: (%lu) '%s'", buf->length, buf->ptr);
          } else if(context & CTX_PRINT) {
            log_parse("Push: EVAL print: (%lu) '%s'", buf->length, buf->ptr);
          } else {
            log_parse("Push: EVAL: (%lu) '%s'", buf->length, buf->ptr);
          }
        //#endif
        
        if((status = elua_push_buf(L, context, &output_started, buf, out, conf))) break;
        
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
        //else { log_parse(filename, line, column, "EVAL == EVAL"); }
      }
      else {
        cstr_appendc(buf, c);
      }
      //else { log_parse(filename, line, column, "EVAL == EVAL %c %c", prev_prev_c, prev_c); }
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
  
  // Reset buffer
  cstr_reset(buf);
  
  return status;
}


int lua_load_cstr(lua_State *L, cstr *buf, const char *filename) {
  return lua_load(L, elua_load_cstr_reader, (void *)buf, filename);
}
