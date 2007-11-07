#ifndef CSTR_H
#define CSTR_H

#define CSTR_VERSION "$Id$"

#include <stdlib.h>

typedef struct {
	char* ptr;
  size_t initial_size;
	size_t size;
	size_t length;
} cstr_t;

cstr_t *cstr_new (size_t capacity);
   void cstr_init (cstr_t *s, size_t capacity);
   void cstr_free (cstr_t *s);
   void cstr_reset (cstr_t *s);
    int cstr_resize (cstr_t *s, const size_t increment);
    int cstr_append (cstr_t *s, const char *src, const size_t srclen);
    int cstr_appends (cstr_t *s, const char *src); // delegates to cstr_append(s, src, strlen(src))
    int cstr_appendc (cstr_t *s, const char ch);
   char cstr_popc (cstr_t *s);

// Append one cstr to another
#define cstr_append_cstr(to, from) \
  cstr_append(to, from->ptr, from->length)


#endif
