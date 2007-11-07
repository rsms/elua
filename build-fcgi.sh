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
. update_version-h.sh

if gcc -g -Wall -O2 -o elua-fcgi cstr.c elua.c elua-fcgi.c -llua -lfcgi -I/opt/local/include -L/opt/local/lib ; then
  if [ ! "$@" == "" ]; then
    lighttpd -Df lighttpd.conf
  fi
fi

exit $?
