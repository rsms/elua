#include <sys/stat.h>

//#include <lua.h>
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


static lua_State *getthread (lua_State *L, int *arg) {
  if (lua_isthread(L, 1)) {
    *arg = 1;
    return lua_tothread(L, 1);
  }
  else {
    *arg = 0;
    return L;
  }
}


// These two come from lua/ldblib.c
#define TB_LEVELS1	12	/* size of the first part of the stack */
#define TB_LEVELS2	10	/* size of the second part of the stack */

static int format_traceback_text (lua_State *L) {
  int level;
  int firstpart = 1;  /* still before eventual `...' */
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  if (lua_isnumber(L, arg+2)) {
    level = (int)lua_tointeger(L, arg+2);
    lua_pop(L, 1);
  }
  else
    level = (L == L1) ? 1 : 0;  /* level 0 may be this own function */
  if (lua_gettop(L) == arg)
    lua_pushliteral(L, "");
  else if (!lua_isstring(L, arg+1)) return 1;  /* message is not a string */
  //else lua_pushliteral(L, "\n");
  //lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (level > TB_LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L1, level+TB_LEVELS2, &ar))
        level--;  /* keep going */
      else {
        lua_pushliteral(L, "  ...\n");  /* too many levels */
        while (lua_getstack(L1, level+TB_LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    lua_pushliteral(L, "  ");
    lua_getinfo(L1, "Snl", &ar);
    lua_pushfstring(L, "%s:", ar.short_src);
    if (ar.currentline > 0)
      lua_pushfstring(L, "%d:", ar.currentline);
    if (*ar.namewhat != '\0')  /* is there a name? */
        lua_pushfstring(L, " in function " LUA_QS, ar.name);
    else {
      if (*ar.what == 'm')  /* main? */
        lua_pushfstring(L, " {main}");
      else if (*ar.what == 'C' || *ar.what == 't')
        lua_pushliteral(L, " ?");  /* C function or tail call */
      else
        lua_pushfstring(L, " in function <%s:%d>",
                           ar.short_src, ar.linedefined);
    }
    lua_pushliteral(L, "\n");
    lua_concat(L, lua_gettop(L) - arg);
  }
  lua_concat(L, lua_gettop(L) - arg);
  return 1;
}


static int format_traceback_xhtml (lua_State *L) {
  // More info: http://pgl.yoyo.org/luai/i/lua_Debug
  int level;
  int firstpart = 1;  /* still before eventual `...' */
  int arg;
  lua_State *L1 = getthread(L, &arg);
  lua_Debug ar;
  if (lua_isnumber(L, arg+2)) {
    level = (int)lua_tointeger(L, arg+2);
    lua_pop(L, 1);
  }
  else
    level = (L == L1) ? 1 : 0;  /* level 0 may be this own function */
  if (lua_gettop(L) == arg)
    lua_pushliteral(L, "");
  else if (!lua_isstring(L, arg+1)) return 1;  /* message is not a string */
  //else lua_pushliteral(L, "\n");
  //lua_pushliteral(L, "stack traceback:");
  while (lua_getstack(L1, level++, &ar)) {
    if (level > TB_LEVELS1 && firstpart) {
      /* no more than `LEVELS2' more levels? */
      if (!lua_getstack(L1, level+TB_LEVELS2, &ar))
        level--;  /* keep going */
      else {
        //lua_pushliteral(L, "<li>...</li>\n");  /* too many levels */
        lua_pushfstring(L, "<li class=\"frame level%d\">...</li>", level-1);
        while (lua_getstack(L1, level+TB_LEVELS2, &ar))  /* find last levels */
          level++;
      }
      firstpart = 0;
      continue;
    }
    //lua_pushliteral(L, "<li class=\"frame\">");
    lua_pushfstring(L, "<li class=\"frame level%d\">", level-1);
    lua_getinfo(L1, "Snl", &ar);
    
    if(ar.source[0] == '@') {
      lua_pushfstring(L, "<em class=\"file\">%s</em>:", ar.short_src);
    } else {
      lua_pushfstring(L, "<em class=\"short_src\">%s</em>:", ar.short_src);
    }
    
    if (ar.currentline > 0)
      lua_pushfstring(L, "<span class=\"line\">%d</span>:", ar.currentline);
    if (*ar.namewhat != '\0')  /* is there a name? */
        lua_pushfstring(L, " in function <b class=\"function\">" LUA_QS "</b>", ar.name);
    else {
      if (*ar.what == 'm')  /* main? */
        lua_pushfstring(L, " <b class=\"function main\">{main}</b>");
      else if (*ar.what == 'C' || *ar.what == 't')
        lua_pushliteral(L, " <b class=\"function unknown\">?</b>");  /* C function or tail call */
      else
        lua_pushfstring(L, " in function <b class=\"function\">%s</b>:<span class=\"line\">%d</span>",
                           ar.short_src, ar.linedefined);
    }
    lua_pushliteral(L, "</li>\n");
    lua_concat(L, lua_gettop(L) - arg);
  }
  lua_concat(L, lua_gettop(L) - arg);
  return 1;
}


static void begin_500_response(const char *title) {
  FCGX_PutStr("Status: 500\r\n"
    "Content-type: text/html\r\n"
    "\r\n"
    "<html><head><title>",
    59, request.out);
  
  if(title == NULL) {
    FCGX_PutStr(
      "500 Internal Error</title></head><body><h1>500 Internal Error",
      61, request.out);
  }
  else {
    FCGX_FPrintF(request.out,
      "%s</title></head><body><h1>%s",
      title, title);
  }
  
  FCGX_PutStr("</h1>", 5, request.out);
}

static void end_500_response() {
  FCGX_FPrintF(request.out,
    "<hr/><address>luwa/" ELUA_VERSION_STRING " r%d</address>"
    "</body></html>", ELUA_REVISION);
}

static int cache_script(lua_State *L, const char *fn, time_t mtime) {
  int status = 0;
	/* 
	 * if func = loadfile("<script>") then return -1
	 *
	 * magnet.cache["<script>"].script = func
	 */

	if((status = elua_loadfile(L, fn, &elua_ctx))) {
    const char *errmsg = lua_tostring (L, -1);
    lua_pop(L, 1); /* remove the error-msg and the function copy from the stack */
    
    FCGX_FPrintF(request.err, "Syntax error: %s", errmsg);
    
    begin_500_response(NULL);
    FCGX_FPrintF(request.out,
      "<h2>Syntax error</h2>"
      "<p>"
        "<tt>%s</tt>"
      "</p>\n", errmsg);
    // TODO: Add source code
    end_500_response();
    
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


static int on_runtime_error(lua_State *L) {
  const char *msg;
  
  // Get error message/description
  msg = luaL_checkstring(L, 1);
  lua_pop(L, 1);
	
	// Retrieve traceback by calling debug.traceback()
	/*lua_getfield(L, LUA_GLOBALSINDEX, "debug");
  lua_getfield(L, -1, "traceback");
  lua_call(L, 0, 1);'*/
  
  // Log to server
  if(format_traceback_text(L)) {
    FCGX_FPrintF(request.err, "Runtime error in %s\n%s", msg, lua_tostring(L, -1));
    lua_pop(L, 1);
  } else {
    FCGX_FPrintF(request.err, "Runtime error in %s", msg);
  }
  
  //if(conf.show_errors) {
    // Send error to client
    // TODO: Examine response.headers and guess what format the traceback should be in
    if(/*conf.error_include_traceback && */format_traceback_xhtml(L)) {
      FCGX_FPrintF(request.out, "\n<h4>Error in %s</h4><ol class=\"traceback\">\n%s</ol>\n", msg, lua_tostring(L, -1));
      lua_pop(L, 1);
    } else {
      FCGX_FPrintF(request.out, "\n<h4>Error in %s</h4>\n", msg);
    }
  //}
	
	// Return
	return 1;
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
		//lua_pushvalue(L, LUA_GLOBALSINDEX); // for lua_setfenv
		lua_setfield(L, -2, "__index"); /* { __index = _G } */
		lua_setmetatable(L, -2); /* setmetatable({}, {__index = _G}) */
		//lua_setfenv(L, -2); /* on the stack should be a modified env */
    
    // XXX temporary
    FCGX_FPrintF(request.out, "Content-type: text/html\r\n\r\n");
    
    // Execute script
    lua_pushcfunction(L, on_runtime_error); // Push error handler
    lua_settop(L, 2);
    lua_insert(L, 1);  // put error function under function to be called
    lua_pcall(L, 0, LUA_MULTRET, 1);
    //lua_pushboolean(L, (st == 0));
    lua_replace(L, 1);
    if(lua_gettop(L)) { // !0 = runtime error (which was handled by on_runtime_error)
      // Runtime error occured
			//lua_pop(L, 1); // pop away error // needed while lua_pushboolean(L, (st == 0)); was enabled
			lua_pop(L, 1);
		}
    
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

