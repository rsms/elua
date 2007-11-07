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
. update_version-h.sh

if gcc -g -Wall -O2 -o elua cstr.c elua.c elua_main.c -llua -I/opt/local/include -L/opt/local/lib ; then
  if [ ! "$@" == "" ]; then
    ./elua $@
  fi
fi

exit $?
