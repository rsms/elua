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

/bin/echo "#ifndef ELUA_VERSION" > version.h
/bin/echo "#define ELUA_VERSION 0x000100 /* 0.1.0 */" >> version.h
/bin/echo "#define ELUA_VERSION_STRING \"0.1.0\"" >> version.h
/bin/echo -n "#define ELUA_REVISION " >> version.h
svnversion|sed -E 's/[^0-9]//g' >> version.h
/bin/echo '#endif' >> version.h

if gcc -g -Wall -O2 -o elua cstr.c elua.c elua_main.c -llua -I/opt/local/include -L/opt/local/lib ; then
  if [ ! "$@" == "" ]; then
    ./elua $@
  fi
fi

exit $?
