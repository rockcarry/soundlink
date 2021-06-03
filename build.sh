#!/bin/sh

set -e

case "$1" in
"")
    gcc fft.c wavfile.c wavdev.c soundlink.c -D_TEST_SOUNDLINK_ -Wall -lwinmm -o soundlink
    ;;
clean)
    rm -rf *.exe
    rm -rf *.wav
    ;;
esac
