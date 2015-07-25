#!/bin/sh

## autoheader
## libtoolize
## aclocal
## automake -a
## autoconf -i -f -Wall

autoreconf -i --force
./configure
make clean
make -j 5
sudo make install
