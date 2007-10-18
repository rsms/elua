#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#include "cstr.h"

#define log_error(fmt, ...) fprintf(stderr, "E " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
//#ifdef DEBUG
#define log_debug(fmt, ...) fprintf(stderr, "D " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
//#else
//#define log_debug(fmt, ...)
//#endif

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
#define CTX_LUA 2
#define CTX_COMMENT 4
#define CTX_PRINT 8

#define log_parse(fmt, ...) fprintf(stdout, "%s %lu:%-2lu  " fmt "\n", filename, line, column, ##__VA_ARGS__)




static cstr buf;

static int elua_push(lua_State *L, const int context) {
  
  if(context & CTX_LUA && context & CTX_COMMENT) {
    /*...*/
  }
  
  //switch(context) {}
  // push line onto lua
  /*error = luaL_loadbuffer(L, buff, strlen(buff), "line") || lua_pcall(L, 0, 0, 0);
  if (error) {
    fprintf(stderr, "%s", lua_tostring(L, -1));
    lua_pop(L, 1);  // pop error message from the stack
  }*/
  cstr_reset(&buf);
  return 0;
}

static int elua_loadfile(lua_State *L, const char *filename, FILE *fp) {
  
  int ch_prev = -1;
  int ch_prev_prev = -1;
  int ch;
  int context = CTX_TEXT;
  int return_status = 0;
  size_t line = 1;
  size_t column = 0;
  
  while(++column) {
    ch = fgetc(fp);
    if(ch == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(fp)) {
        log_error("I/O Error #%d: %s", errno, get_errno_msg());
        return_status = errno;
      } // else: EOF
      break;
    }
    
    if(context & CTX_TEXT) {
      if(ch_prev == '<' && ch == '%') {
        log_parse("Switch: TEXT -> LUA <%%");
        cstr_popc(&buf); // remove '<'
        log_parse("Push: Text: (%lu) '%s'", buf.length, buf.ptr);
        elua_push(L, context);
        context = CTX_LUA;
      }
      else {
        cstr_appendc(&buf, ch);
      }
    }
    else if(context & CTX_LUA) {
      if(ch_prev == '%' && ch == '>') {
        log_parse("Switch: LUA -> TEXT %%>");
        cstr_popc(&buf); // remove '%'
        //#ifdef DEBUG
        if(context & CTX_COMMENT) {
          log_parse("Push: Comment: (%lu) '%s'", buf.length, buf.ptr);
        } else if(context & CTX_PRINT) {
          log_parse("Push: Lua-print: (%lu) '%s'", buf.length, buf.ptr);
        } else {
          log_parse("Push: Lua-eval: (%lu) '%s'", buf.length, buf.ptr);
        }
        //#endif
        elua_push(L, context);
        context = CTX_TEXT;
      }
      else if(ch_prev_prev == '<' && ch_prev == '%') {
        // the first char after "<%"
        if(ch == '#') {
          context |= CTX_COMMENT;
          log_parse("Switch: LUA -> COMMENT <%%#");
        }
        else if(ch == '=') {
          context |= CTX_PRINT;
          log_parse("Switch: LUA -> PRINT <%%=");
        } 
        //else { log_parse(filename, line, column, "LUA == LUA"); }
      }
      else {
        cstr_appendc(&buf, ch);
      }
      //else { log_parse(filename, line, column, "LUA == LUA %c %c", ch_prev_prev, ch_prev); }
    }
    //else { log_debug("nomatch %d (%d, %d)", context & CTX_LUA, context, CTX_LUA); }
    
    if(ch == '\n') {
      line++;
      column = 0;
    }
    
    ch_prev_prev = ch_prev;
    ch_prev = ch;
  }
  
  lua_pop(L, 1);
  cstr_reset(&buf);
  return return_status;
}


int main (int argc, char const *argv[]) {
  
  lua_State *L;
  int status;
  const char *filename;
  const char *stdin_filename;
  FILE *fp;
  
  fp = stdin;
  buf = cstr_new(20*1024);
  L = lua_open();
  luaL_openlibs(L);
  stdin_filename = "<stdin>";
  filename = stdin_filename;
  
  // Filename from args?
  if(argc > 1) {
    filename = argv[1]; // not very safe, but who cares, really..
    if(filename[0] == '-') {
      if(strcmp(filename, "--version") == 0 || strcmp(filename, "-v") == 0) {
        fprintf(stdout, "elua %s, built %s %s\n", "$Id$", __DATE__, __TIME__);
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
  status = elua_loadfile(L, filename, fp);
  
  // Close down
  fclose(fp);
  lua_close(L);
  return status;
}
