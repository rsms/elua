#include "cstr.h"
#include <string.h>

/* $Id$ */

cstr cstr_new(size_t size) {
  cstr s = {(unsigned char *)malloc(sizeof(char)*(size+1)), size, size, 0};
  s.ptr[0] = 0;
  return s;
}

void cstr_free(cstr *s) {
  free(s->ptr);
}

void cstr_reset(cstr *s) {
  s->length = 0;
  s->ptr[s->length] = 0;
}

int cstr_resize(cstr *s, const size_t increment) {
  size_t new_size;
  if(increment < s->initial_size) {
		new_size = s->size + s->initial_size + 1;
	} else {
	  new_size = s->size + increment + 1;
	}
  unsigned char *new = (unsigned char *)realloc(s->ptr, sizeof(char)*new_size);
  if(new != NULL) {
    s->ptr = new;
    s->size = new_size;
  } else {
    return 1;
  }
  return 0;
}

int cstr_append(cstr *s, const unsigned char *src, const size_t srclen) {
  if(s->size - s->length <= srclen) {
    if(!cstr_resize(s, srclen)) {
      return 1;
    }
  }
  memcpy(s->ptr + s->length, src, srclen);
  s->length += srclen;
  s->ptr[s->length] = 0;
  return 0;
}

int cstr_appendc(cstr *s, const unsigned char ch) {
  if(s->length >= s->size) {
    if(cstr_resize(s, (size_t)1)) {
      return 1;
    }
  }
  s->ptr[s->length++] = ch;
  s->ptr[s->length] = 0;
  return 0;
}

unsigned char cstr_popc(cstr *s) {
  if(s->length) {
    unsigned char ch = s->ptr[s->length--];
    s->ptr[s->length] = 0;
    return ch;
  }
  return (unsigned char)0;
}