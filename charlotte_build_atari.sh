#!/bin/sh

set -ex

export CC=m68k-atari-mint-gcc
export AR=m68k-atari-mint-ar
export STRIP=m68k-atari-mint-strip

make 
