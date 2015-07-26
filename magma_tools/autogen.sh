#!/bin/sh

autoheader
# libtoolize
aclocal
automake -a
autoconf -f -Wall
