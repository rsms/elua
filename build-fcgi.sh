#!/bin/sh
# $Id$
#############################################
# Just compile:
# ./build.sh
#
# Build and run lighttpd in foreground
# ./build.sh anythinghere
#
#############################################

cd `dirname "$0"`

/bin/echo "#ifndef ELUA_VERSION" > version.h
/bin/echo "#define ELUA_VERSION 0x000100 /* 0.1.0 */" >> version.h
/bin/echo "#define ELUA_VERSION_STRING \"0.1.0\"" >> version.h
/bin/echo -n "#define ELUA_REVISION " >> version.h
svnversion|sed -E 's/[^0-9]//g' >> version.h
/bin/echo '#endif' >> version.h

if gcc -g -Wall -O2 -o elua-fcgi cstr.c elua.c elua-fcgi.c -llua -lfcgi -I/opt/local/include -L/opt/local/lib ; then
  if [ ! "$@" == "" ]; then
    lighttpd -Df lighttpd.conf
  fi
fi

exit $?
