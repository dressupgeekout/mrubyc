#!/bin/sh

set -ex

export CC=m68k-atari-mint-gcc
export AR=m68k-atari-mint-ar
export STRIP=m68k-atari-mint-strip
export RANLIB=m68k-atari-mint-ranlib

make clean
make mrubyc_lib ##MRBC_USE_HAL_POSIX=1
${RANLIB} src/libmrubyc.a
make -C atari RUBY.TOS
