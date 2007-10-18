#!/bin/sh
# $Id$
#############################################
# Just compile:
# ./build.sh
#
# Build and run on test.elua:
# ./build.sh test.elua
#
# Build and pass some other argument(s) to the elua binary:
# ./build.sh --version
#
#############################################

cd `dirname "$0"`

echo "#ifndef ELUA_VERSION" > version.h
echo -n "#define ELUA_VERSION " >> version.h
svn info . 2> /dev/null|grep 'Revision: '|cut -d ' ' -f 2 >> version.h
echo '#endif' >> version.h

if gcc -g -Wall -O2 -o elua elua.c cstr.c -llua -I/opt/local/include -L/opt/local/lib ; then
  if [ ! "$@" == "" ]; then
    ./elua $@
  fi
fi
