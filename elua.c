#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <lualib.h>
#include <lauxlib.h>

#define log_error(fmt, ...) fprintf(stderr, "E " __FILE__ ":%d: " fmt "\n", __LINE__, ##__VA_ARGS__)
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

#define log_parse(fmt, ...) fprintf(stderr, "%s %lu:%-2lu  " fmt "\n", filename, line, column, ##__VA_ARGS__)

#define ELUA_LOADFILE_BUFMINSIZE 1024

typedef struct {
	unsigned char* ptr;
	size_t size;
	size_t length;
} cstr;

static cstr cstr_new(size_t size) {
  cstr s = {(unsigned char *)malloc(sizeof(char)*(size+1)), size, 0};
  s.ptr[0] = 0;
  return s;
}

static void cstr_free(cstr *s) {
  free(s->ptr);
}

static void cstr_reset(cstr *s) {
  s->length = 0;
  s->ptr[s->length] = 0;
}

static int cstr_resize(cstr *s, const size_t increment) {
  size_t new_size = s->size + increment + 1;
  unsigned char *new = (unsigned char *)realloc(s->ptr, sizeof(char)*new_size);
  if(new != NULL) {
    s->ptr = new;
    s->size = new_size;
  } else {
    return 1;
  }
  return 0;
}

static int cstr_append(cstr *s, const unsigned char *src, const size_t srclen) {
  if(s->size - s->length < srclen) {
    if(!cstr_resize(s, srclen)) {
      return 1;
    }
  }
  memcpy(s->ptr + s->length, src, srclen);
  s->length += srclen;
  s->ptr[s->length] = 0;
  return 0;
}

static int cstr_appendc(cstr *s, const unsigned char ch) {
  if(s->length >= s->size) {
    if(!cstr_resize(s, (size_t)1)) {
      return 1;
    }
  }
  s->ptr[s->length++] = ch;
  s->ptr[s->length] = 0;
  return 0;
}

static unsigned char cstr_popc(cstr *s) {
  if(s->length) {
    unsigned char ch = s->ptr[s->length--];
    s->ptr[s->length] = 0;
    return ch;
  }
  return (unsigned char)0;
}

static int elua_loadfile(lua_State *L, const char *filename) {
  
  FILE *fp = fopen(filename, "r");
  if(fp == NULL) {
    log_error("Failed to open file '%s' for reading", filename);
    return 1;
  }
  
  int ch_prev = -1;
  int ch_prev_prev = -1;
  int ch;
  int context = CTX_TEXT;
  int return_status = 0;
  size_t line = 1;
  size_t column = 0;
  cstr buf = cstr_new(20*1024);
  
  while(++column) {
    ch = fgetc(fp);
    if(ch == EOF) {
      log_debug("EOF @ column %lu, line %lu", column, line);
      if(ferror(fp)) {
        log_error("I/O Error #%d: %s", errno, get_io_error_msg());
        return_status = errno;
      } // else: EOF
      break;
    }
    
    if(context & CTX_TEXT) {
      if(ch_prev == '<' && ch == '%') {
        context = CTX_LUA;
        log_parse("TEXT -> LUA <%%");
        cstr_popc(&buf); // remove '<'
        log_parse("OUT:Text: (%lu) %s", buf.length, buf.ptr);
        // push line onto lua
        cstr_reset(&buf);
      }
      else {
        cstr_appendc(&buf, ch);
      }
    }
    else if(context & CTX_LUA) {
      if(ch_prev == '%' && ch == '>') {
        context = CTX_TEXT;
        log_parse("LUA -> TEXT %%>");
        
        if(context & CTX_COMMENT) {
          log_parse("OUT:Comment: %s", buf.ptr);
        }
        else {
          cstr_popc(&buf); // remove '<'
          log_parse("OUT:Lua: (%lu) %s", buf.length, buf.ptr);
          // push line onto lua
          /*error = luaL_loadbuffer(L, buff, strlen(buff), "line") || lua_pcall(L, 0, 0, 0);
          if (error) {
            fprintf(stderr, "%s", lua_tostring(L, -1));
            lua_pop(L, 1);  // pop error message from the stack
          }*/
          cstr_reset(&buf);
        }
      }
      else if(ch_prev_prev == '<' && ch_prev == '%') {
        // the first char after "<%"
        if(ch == '#') {
          context |= CTX_COMMENT;
          log_parse("LUA -> COMMENT <%%#");
        }
        else if(ch == '=') {
          context |= CTX_PRINT;
          log_parse("LUA -> PRINT <%%=");
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
  fclose(fp);
  return return_status;
}


/*static int elua_print(lua_State *L) {
  const char *s = luaL_checkstring(L, 1);
  printf("%s", s);
  return 0;
}*/


int main (int argc, char const *argv[]) {
  
  lua_State *L;
  int status;
  
  L = lua_open();
  luaL_openlibs(L);
  
  status = elua_loadfile(L, "test.elua");
  
  lua_close(L);
  
  return status;
}
