#!/bin/sh

# autoheader
# libtoolize
# aclocal
# automake -a
# autoconf -f -Wall

autoreconf -i --force
./configure
make clean
make -j 5
sudo make install
