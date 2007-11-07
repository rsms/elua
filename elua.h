#ifndef ELUA_H
#define ELUA_H

#include <lualib.h>
#include "cstr.h"

// Configuration/options
typedef struct {
  // If enabled, comments in ELUA will not propagate to LUA. Default: 1
  char strip_comments;
  // If enabled, inputs a call to response.begin() before the first print statement is found. Default: 0
  char auto_begin_response_call;
} elua_conf_t;

// Context
typedef struct {
  cstr_t *buf;
  cstr_t *out;
  elua_conf_t *conf;
} elua_context_t;


// Initialize a new context.
// Also causes configuration to be initialized with default values, so no
// need to call elua_reset_conf, unless you want to reset the configuration.
void elua_init_context (elua_context_t *ctx);

// Free an initialized context
void elua_free_context (elua_context_t *ctx);

// Reset configuration with default values.
void elua_reset_conf (elua_conf_t *conf);


/**
 * Parse from an open file.
 * 
 * @param  L          The LUA stack/state
 * @param  f          An open FD
 * @param  buf        Temporary read buffer
 * @param  out        Script buffer - this is where the final LUA-script will be after successful return.
 * @param  conf       ELUA configuration
 * @return Error code or 0 if no errors
 */
int elua_parse_file (lua_State *L, FILE *f, elua_context_t *ctx);

// Load a ELUA file
// Convenience function.
int elua_loadfile (lua_State *L, const char *fn, elua_context_t *ctx);


/**
 * Load LUA-code from a cstr into the current state L.
 *
 * This is exactly like luaL_loadbuffer or luaL_loadstring, but loading from a
 * cstr.
 * 
 * @param  L          The LUA stack/state
 * @param  buf        Code to load
 * @param  filename   Name of the file being parsed. Prefix with '@' to get 
 *                    pretty error messages.
 * @return Error code or 0 if no errors
 */
int lua_load_cstr (lua_State *L, cstr_t *buf, const char *filename);


#endif
