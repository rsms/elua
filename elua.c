#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>
//#define DEBUG 1
#include "cstr.h"
#include "version.h"
#include "macros.h"

// Note:
// XXX Includes, see: http://pgl.yoyo.org/luai/i/dofile

static const char* get_errno_msg() {
  switch(errno) {
    case 0: return "No error";
    case EACCES: return "Another process has the file locked";
    case EBADF: return "stream is not a valid stream opened for reading";
    case EINTR: return "A signal interrupted the call";
    case EIO: return "An input error occurred";
    case EISDIR: return "The open object is a directory, not a file";
    case ENOMEM: return "Memory could not be allocated for internal buffers";
    case ENXIO: return "A device error occurred";
    case EOVERFLOW:  return "The file is a regular file and an attempt was made "
                            "to read at or beyond the offset maximum associated "
                            "with the corresponding stream";
    case EWOULDBLOCK:  return "The underlying file descriptor is a non-blocking "
                              "socket and no data is ready to be read";
  }
  return "Unknown";
}

#define CTX_TEXT 1
#define CTX_EVAL 2
#define CTX_COMMENT 4
#define CTX_PRINT 8
#define CTX_OUTPUT_STARTED 16
#define CTX_MULTILINE 32

#ifdef DEBUG
#define log_parse(fmt, ...) fprintf(stdout, "%s %lu:%-2lu  " fmt "\n", filename, line, column, ##__VA_ARGS__)
#else
#define log_parse(fmt, ...)
#endif

/**
 * Push something onto the lua stack
 */
static int elua_push_buf(lua_State *L, const int context, int output_started, cstr *buf) {
  
  if(buf->length == 0) {
    return output_started;
  }
  
  if( (!output_started) && ((context & CTX_PRINT) || (!(context & CTX_EVAL))) ) {
    //compiler->out = rb_str_buf_cat(compiler->out, "send_headers!\n", 14);
    //log_debug("push: send_headers()");
    output_started = 1;
  }
  
  if(context & CTX_EVAL) {
    if(context & CTX_COMMENT) {
      log_debug("Push: comment: (%lu) '%s'", buf->length, buf->ptr);
      if(context & CTX_MULTILINE) {
        printf("--[[%s]]\n", buf->ptr);
      }
      else {
        printf("--%s\n", buf->ptr);
      }
      // discard
    }
    else if(context & CTX_PRINT) {
      log_debug("Push: print: (%lu) '%s'", buf->length, buf->ptr);
      cstr_appendc(buf, ')');
      cstr_appendc(buf, '\n');
      printf("io.write(%s", buf->ptr);
      //compiler->out = rb_str_buf_cat(out, "@out.write((", 12);
      //compiler->out = rb_str_buf_cat(out, buf->ptr, buf->length);
    }
    else {
      log_debug("Push: eval: (%lu) '%s'", buf->length, buf->ptr);
      cstr_appendc(buf, '\n');
      printf("%s", buf->ptr);
      //compiler->out = rb_str_buf_cat(compiler->out, buf->ptr, buf->length);
    }
  }
  else {
    log_debug("Push: text: (%lu) '%s'", buf->length, buf->ptr);
    cstr_appendc(buf, '"');
    cstr_appendc(buf, ')');
    cstr_appendc(buf, '\n');
    printf("io.write(\"%s", buf->ptr);
    //compiler->out = rb_str_buf_cat(compiler->out, "@out.write('", 12);
    //compiler->out = rb_str_buf_cat(compiler->out, buf->ptr, buf->length);
  }
  
  cstr_reset(buf);
  return output_started;
}

//int _elua_push_cb

/**
 * @param  L          The LUA stack/state
 * @param  filename   Filename of the file or input being parsed
 * @param  f          An open FD
 * @param  buf        Buffer to use. Can be an already used buffer (reuse)
 * @return 0 on success, <>0 on error
 */
static int elua_loadfile(lua_State *L, const char *filename, FILE *f, cstr *buf) {
  
  log_debug("Entered elua_loadfile");
  
  int c;
  int prev_c = -1;
  int prev_prev_c = -1;
  int context = CTX_TEXT;
  int return_status = 0;
  int output_started = 0;
  size_t line = 1;
  size_t column = 0;
  
  // Make sure buf is reset
  cstr_reset(buf);
  
  log_debug("Entering read-loop");
  while(++column) {
    c  = fgetc(f); // We might want to wrap this in TRAP_BEG .. TRAP_END
    // Handle EOF
    if(c == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(f)) {
        log_error("I/O Error #%d: %s", errno, get_errno_msg());
        clearerr(f);
        return_status = errno;
      }
      else if(buf->length != 0) {
        output_started = elua_push_buf(L, context, output_started, buf);
      }
      break; 
    }
    
    // In text context?
    if(context & CTX_TEXT) {
      if(prev_c == '<' && c == '%') {
        log_parse("Switch: TEXT -> EVAL <%%");
        cstr_popc(buf); // remove '<'
        log_parse("Push: TEXT: (%lu) '%s'", buf->length, buf->ptr);
        output_started = elua_push_buf(L, context, output_started, buf);
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
        output_started = elua_push_buf(L, context, output_started, buf);
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
  
  cstr_reset(buf);
  return return_status;
}

/* ------------------------------------------------ */

int main (int argc, char const *argv[]) {
  
  lua_State   *L;
  cstr        buf;
  int         status;
  const char  *filename;
  const char  *stdin_filename;
  FILE        *fp;
  
  fp              = stdin;
  buf             = cstr_new(4096);
  L               = lua_open();
  luaL_openlibs(L);
  stdin_filename  = "<stdin>";
  filename        = stdin_filename;
  
  // Filename from args?
  if(argc > 1) {
    filename = argv[1]; // not very safe, but who cares, really..
    if(filename[0] == '-') {
      if(strcmp(filename, "--version") == 0 || strcmp(filename, "-v") == 0) {
        fprintf(stdout, "elua %s r%d, built %s %s\n", ELUA_VERSION_STRING, ELUA_REVISION, __DATE__, __TIME__);
        exit(0);
      }
      else {
        fprintf(stderr, "Usage: %s [options|FILE]\n"
          "FILE\n"
          "  The name of a elua-file to execute. Read from stdin if not specified.\n"
          "\n"
          "Options:\n"
          "  -h --help     Show this message to stderr and exit.\n"
          "  -v --version  Print version to stdout and exit.\n"
          "  -p --print    Print LUA code to stdout instead of running it.\n"
          ,argv[0]);
        exit(1);
      }
    }
  }
  
  // Open file
  if(filename != stdin_filename) {
    fp = fopen(filename, "r");
    if(fp == NULL) {
      log_error("Failed to open file '%s' for reading. (Reason: %s)", filename, get_errno_msg());
      return 1;
    }
  }
  
  // Parse & load
  status = elua_loadfile(L, filename, fp, &buf);
  
  // Close down
  fclose(fp);
  cstr_free(&buf);
  lua_close(L);
  return status;
}
