#!/bin/sh

if gcc -g -Wall -O2 -o elua elua.c -llua -I/opt/local/include -L/opt/local/lib ; then
  ./elua
fi
