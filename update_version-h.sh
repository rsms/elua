#!/bin/sh
/bin/echo "#ifndef ELUA_VERSION" > version.h
/bin/echo "#define ELUA_VERSION 0x000100 /* 0.1.0 */" >> version.h
/bin/echo "#define ELUA_VERSION_STRING \"0.1.0\"" >> version.h
/bin/echo -n "#define ELUA_REVISION " >> version.h
svnversion|sed -E 's/([0-9]+).*/\1/g' >> version.h
/bin/echo '#endif' >> version.h