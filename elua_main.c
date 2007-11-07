#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <lauxlib.h>

#include "version.h"
#include "macros.h"
#include "elua.h"

// TODO: getopt and add support for outputing LUA instead of running it

int main (int argc, char const *argv[]) {
  
  lua_State   *L;
  elua_conf_t conf;
  cstr        buf;
  cstr        out;
  int         error;
  const char  *filename;
  char        *filename2;
  size_t      filename_size;
  const char  *stdin_filename;
  FILE        *fp;
  
  conf            = elua_conf_defaults();
  fp              = stdin;
  buf             = cstr_new(4096);
  out             = cstr_new(4096);
  L               = lua_open();
  luaL_openlibs(L);
  stdin_filename  = "<stdin>";
  filename        = stdin_filename;
  
  // Filename from args?
  if(argc > 1) {
    filename = argv[1]; // not very pretty, but who cares, really...
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
          ,argv[0]);
        exit(1);
      }
    }
  }
  
  // Filename with @-prefix
  filename_size = strlen(filename);
  filename2 = (char *)malloc(filename_size+2);
  filename2[0] = '@';
  memcpy(filename2+1, filename, filename_size);
  filename2[filename_size+1] = 0;
  
  // Open file
  if(filename != stdin_filename) {
    fp = fopen(filename, "r");
    if(fp == NULL) {
      log_error("Failed to open file '%s' for reading. (Reason: %s)", filename, strerror(errno));
      return 1;
    }
  }
  
  // Parse & Load
  if( (error = elua_parse_file(L, fp, &buf, &out, &conf)) ) {
    fprintf(stderr, "Load error: %s\n", lua_tostring(L, -1));
		lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */
  }
  else {
    // Evaluate into the LUA stack
    if( (error = lua_load_cstr(L, &out, filename2)) ) {
      fprintf(stderr, "Syntax error: %s\n", lua_tostring(L, -1));
		  lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */
    }
    else {
      // Run
      if( (error = lua_pcall(L, 0, LUA_MULTRET, 0)) ) {
        const char *errmsg = lua_tostring(L, -1);
        // TODO: Print source only for the rows where the error occured, like python does.
        fprintf(stderr, "Runtime error: %s\n%s\n", errmsg, out.ptr);
  			lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */
      }
    }
  }
  
  // Close down
  fclose(fp);
  cstr_free(&buf);
  cstr_free(&out);
  lua_close(L);
  free(filename2);
  
  return error;
}
