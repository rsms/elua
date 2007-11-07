#include "cstr.h"
#include <string.h>

/* $Id$ */

/*
typedef struct {
	char* ptr;
  size_t initial_size;
	size_t size;
	size_t length;
} cstr_t;
*/

cstr_t *cstr_new(size_t capacity) {
  cstr_t *s = (cstr_t *)malloc(sizeof(cstr_t));
  cstr_init(s, capacity);
  return s;
}

void cstr_init (cstr_t *s, size_t capacity) {
  s->ptr = (char *)malloc(sizeof(char)*(capacity+1));
  s->ptr[0] = 0;
  s->initial_size = capacity;
  s->size = capacity;
  s->length = 0;
}

void cstr_free(cstr_t *s) {
  if(s->ptr) {
    free(s->ptr);
  }
}

void cstr_reset(cstr_t *s) {
  s->length = 0;
  s->ptr[s->length] = 0;
}

int cstr_resize(cstr_t *s, const size_t increment) {
  size_t new_size;
  if(increment < s->initial_size) {
		new_size = s->size + s->initial_size + 1;
	} else {
	  new_size = s->size + increment + 1;
	}
  char *new = (char *)realloc(s->ptr, sizeof(char)*new_size);
  if(new != NULL) {
    s->ptr = new;
    s->size = new_size;
  } else {
    return 1;
  }
  return 0;
}

int cstr_append(cstr_t *s, const char *src, const size_t srclen) {
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

int cstr_appends (cstr_t *s, const char *src) {
  return cstr_append(s, src, strlen(src));
}

int cstr_appendc(cstr_t *s, const char ch) {
  if(s->length >= s->size) {
    if(cstr_resize(s, (size_t)1)) {
      return 1;
    }
  }
  s->ptr[s->length++] = ch;
  s->ptr[s->length] = 0;
  return 0;
}

char cstr_popc(cstr_t *s) {
  if(s->length) {
    char ch = s->ptr[s->length--];
    s->ptr[s->length] = 0;
    return ch;
  }
  return (char)0;
}