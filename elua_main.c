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
  elua_context_t elua_ctx;
  int         status;
  const char  *filename;
  const char  *stdin_filename;
  FILE        *fp;
  
  status = 0;
  fp = stdin;
  L = lua_open();
  stdin_filename = "<stdin>";
  filename = stdin_filename;
  elua_init_context(&elua_ctx);
  luaL_openlibs(L);
  
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
  
  // TODO: restore stdin-support
  
  // Load
  if((status = elua_loadfile(L, filename, &elua_ctx))) {
    fprintf(stderr, "Load error: %s\n", lua_tostring(L, -1));
	  lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */
	}
  else { // Run
    if( (status = lua_pcall(L, 0, LUA_MULTRET, 0)) ) {
      // TODO: Print source only for the rows where the error occured, like python does.
      fprintf(stderr, "Runtime error: %s\n%s\n", lua_tostring(L, -1), elua_ctx.out->ptr);
			lua_pop(L, 1);
    }
  }
  
  // Clean-up
  elua_free_context(&elua_ctx);
  lua_close(L);
  
  return status;
}
