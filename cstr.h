#ifndef CSTR_H
#define CSTR_H

#define CSTR_VERSION "$Id$"

#include <stdlib.h>

typedef struct {
	unsigned char* ptr;
  size_t initial_size;
	size_t size;
	size_t length;
} cstr;

cstr cstr_new (size_t size);
void cstr_free (cstr *s);
void cstr_reset (cstr *s);
int cstr_resize (cstr *s, const size_t increment);
int cstr_append (cstr *s, const unsigned char *src, const size_t srclen);
int cstr_appendc (cstr *s, const unsigned char ch);
unsigned char cstr_popc (cstr *s);

#endif
