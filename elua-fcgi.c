#include <sys/stat.h>

#include <lualib.h>
#include <lauxlib.h>

#include <fcgiapp.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "version.h"
#include "macros.h"
#include "elua.h"

// Since this is single-threaded...
static struct {
  FCGX_Stream *in;
  FCGX_Stream *out;
  FCGX_Stream *err;
  FCGX_ParamArray env;
} request;

elua_context_t elua_ctx;

static int cache_script(lua_State *L, const char *fn, time_t mtime) {
  int status = 0;
	/* 
	 * if func = loadfile("<script>") then return -1
	 *
	 * magnet.cache["<script>"].script = func
	 */

	if((status = elua_loadfile(L, fn, &elua_ctx))) {
    FCGX_FPrintF(request.err, "Syntax error: %s\n", lua_tostring(L, -1));
	  lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */
		return status;
	}

	lua_getfield(L, LUA_GLOBALSINDEX, "elua"); 
	lua_getfield(L, -1, "cache");

	/* .script = <func> */
	lua_newtable(L); 
	assert(lua_isfunction(L, -4));
	lua_pushvalue(L, -4); 
	lua_setfield(L, -2, "script");

	lua_pushinteger(L, mtime);
	lua_setfield(L, -2, "mtime");

	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "hits");

	/* magnet.cache["<script>"] = { script = <func> } */
	lua_setfield(L, -2, fn);

	lua_pop(L, 2); /* pop magnet and magnet.cache */

	/* on the stack should be the function itself */
	assert(lua_isfunction(L, lua_gettop(L)));

	return 0;
}

static int get_script(lua_State *L, const char *fn) {
	struct stat st;
	time_t mtime = 0;

	assert(lua_gettop(L) == 0);

	if (-1 == stat(fn, &st)) {
		return -1;
	}

	mtime = st.st_mtime;

	/* if not magnet.cache["<script>"] then */

	lua_getfield(L, LUA_GLOBALSINDEX, "elua"); 
	assert(lua_istable(L, -1));
	lua_getfield(L, -1, "cache");
	assert(lua_istable(L, -1));

	lua_getfield(L, -1, fn);

	if (!lua_istable(L, -1)) {
		lua_pop(L, 3); /* pop the nil */

		if (cache_script(L, fn, mtime)) {
			return -1;
		}
	} else {
		lua_getfield(L, -1, "mtime");
		assert(lua_isnumber(L, -1));

		if (mtime == lua_tointeger(L, -1)) {
			lua_Integer hits;

			lua_pop(L, 1);
	
			/* increment the hit counter */	
			lua_getfield(L, -1, "hits");
			hits = lua_tointeger(L, -1);
			lua_pop(L, 1);
			lua_pushinteger(L, hits + 1);
			lua_setfield(L, -2, "hits");

			/* it is in the cache */
			assert(lua_istable(L, -1));
			lua_getfield(L, -1, "script");
			assert(lua_isfunction(L, -1));
			lua_insert(L, -4);
			lua_pop(L, 3);
			assert(lua_isfunction(L, -1));
		} else {
			lua_pop(L, 1 + 3);

			if (cache_script(L, fn, mtime)) {
				return -1;
			}
		}

		/* this should be the function */
	}
	assert(lua_gettop(L) == 1);

	return 0;
}

static void print_env(FCGX_Stream *out, char *label, char **envp) {
    FCGX_FPrintF(out, "%s:<br>\n<pre>\n", label);
    for( ; *envp != NULL; envp++) {
        FCGX_FPrintF(out, "%s\n", *envp);
    }
    FCGX_FPrintF(out, "</pre><p>\n");
}

static int response_print(lua_State *L) {
	FCGX_PutS(luaL_checkstring(L, 1), request.out);
	return 0;
}

int main (int argc, char const *argv[]) {
  int status;
  lua_State   *L;
  
  // Init
  status = 0;
  elua_init_context(&elua_ctx); // elua_ctx is global
  L = lua_open();
  luaL_openlibs(L);
  
  // Setup cache table
  lua_newtable(L); // elua.
	lua_newtable(L); // elua.cache.
	lua_setfield(L, -2, "cache");
	lua_setfield(L, LUA_GLOBALSINDEX, "elua");
  
  // We have to overwrite the print function
  lua_pushcfunction(L, response_print);                       /* (sp += 1) */
  lua_setfield(L, LUA_GLOBALSINDEX, "print"); /* -1 is the env we want to set(sp -= 1) */
  
  // Process requests
  while (FCGX_Accept(&request.in, &request.out, &request.err, &request.env) >= 0) {
		assert(lua_gettop(L) == 0);
		
		if (get_script(L, FCGX_GetParam("SCRIPT_FILENAME", request.env))) {
			printf("Status: 404\r\n\r\n404 Not Found");
			assert(lua_gettop(L) == 0);
			continue;
		}
		
		/**
		 * we want to create empty environment for our script 
		 * 
		 * setmetatable({}, {__index = _G})
		 * 
		 * if a function, symbol is not defined in our env, __index will lookup 
		 * in the global env.
		 *
		 * all variables created in the script-env will be thrown 
		 * away at the end of the script run.
		 */
		lua_newtable(L); /* my empty environment */
    
    
    /*lua_newtable(L); // response.
    lua_pushcfunction(L, response_print);
  	lua_setfield(L, -2, "write");
  	lua_setfield(L, LUA_GLOBALSINDEX, "response");
    lua_pop(L, 1);*/
    
		/* we have to overwrite the print function */
		//lua_pushcfunction(L, response_print);                       /* (sp += 1) */
		//lua_setfield(L, -2, "print"); /* -1 is the env we want to set(sp -= 1) */
    
		lua_newtable(L); /* the meta-table for the new env  */
		lua_pushvalue(L, LUA_GLOBALSINDEX);
		lua_setfield(L, -2, "__index"); /* { __index = _G } */
		lua_setmetatable(L, -2); /* setmetatable({}, {__index = _G}) */
		//lua_setfenv(L, -2); /* on the stack should be a modified env */
    
    // XXX temporary
    FCGX_FPrintF(request.out, "Content-type: text/html\r\n\r\n");
    
		if (lua_pcall(L, 0, 1, 0)) {
      const char *errstr = lua_tostring(L, -1);
		  FCGX_FPrintF(request.err, "%s\n", errstr);
			log_error("%s\n", errstr);
			lua_pop(L, 1);
			continue;
		}
    
		lua_pop(L, 1);
		assert(lua_gettop(L) == 0);
    
    /*FCGX_FPrintF(request.out,
      "Content-type: text/html\r\n"
      "\r\n"
      "<title>elua-fcgi</title>"
      "<h1>elua-fcgi</h1>\n"
      "Filename: %s\n"
      "Process ID: %d<p>\n", filename.ptr, getpid());
    
    print_env(request.out, "Request environment", request.env);*/
  }
  
  return status;
}

