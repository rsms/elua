#include <stdio.h>
#include <string.h>
#include <errno.h>

#define log_error(fmt, ...) fprintf(stderr, "E " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
#define log_parse(file, line, col, fmt, ...) fprintf(stderr, "%s %lu:%-2lu  " fmt "\n", file, line, col, ##__VA_ARGS__)
//#ifdef DEBUG
#define log_debug(fmt, ...) fprintf(stderr, "D " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
//#else
//#define log_debug(fmt, ...)
//#endif

static const char* get_io_error_msg() {
  switch(errno) {
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

static int parse_file(const char *filename, FILE *out) {
  
  FILE *fp = fopen(filename, "r");
  if(fp == NULL) {
    log_error("Failed to open file '%s' for reading", filename);
    return 1;
  }
  
  int ch_prev = -1;
  int ch_prev_prev = -1;
  int ch;
  int context = CTX_TEXT;
  size_t line = 1;
  size_t column = 0;
  
  while(++column) {
    ch = fgetc(fp);
    if(ch == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(fp)) {
        log_error("I/O Error #%d: %s", errno, get_io_error_msg());
      } // else: EOF
      break;
    }
    
    if(context & CTX_TEXT && ch_prev == '<' && ch == '%') {
      context = CTX_LUA;
      log_parse(filename, line, column, "TEXT -> LUA <%%");
    }
    else if(context & CTX_LUA) {
      if(ch_prev == '%' && ch == '>') {
        context = CTX_TEXT;
        log_parse(filename, line, column, "LUA -> TEXT %%>");
      }
      else if(ch_prev_prev == '<' && ch_prev == '%') {
        // the first char after "<%"
        if(ch == '#') {
          context |= CTX_COMMENT;
          log_parse(filename, line, column, "LUA -> COMMENT <%%#");
        }
        else if(ch == '=') {
          context |= CTX_PRINT;
          log_parse(filename, line, column, "LUA -> PRINT <%%=");
        } 
        //else { log_parse(filename, line, column, "LUA == LUA"); }
      }
      //else { log_parse(filename, line, column, "LUA == LUA %c %c", ch_prev_prev, ch_prev); }
    }
    
    
    if(ch == '\n') {
      line++;
      column = 0;
    }
    
    ch_prev_prev = ch_prev;
    ch_prev = ch;
  }
  
  fclose(fp);
  return 0;
}


int main (int argc, char const *argv[]) {
  return parse_file("test.elua", stdout);
}
