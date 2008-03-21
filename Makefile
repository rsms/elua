#!/usr/bin/env make
all:  elua elua-fcgi

elua:
	gcc -g -Wall -O2 -o elua cstr.c elua.c elua_main.c -llua -I/opt/local/include -L/opt/local/lib

elua-fcgi:
	gcc -g -Wall -O2 -o elua-fcgi cstr.c elua.c elua-fcgi.c -llua -lfcgi -I/opt/local/include -L/opt/local/lib

clean:
	rm -r elua.dSYM/
	rm -r elua-fcgi.dSYM/

test:
	./elua tests/test.elua


.PHONY: compile test
