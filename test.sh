#!/usr/bin/env bash

if [ ! -d "./bin" ]; then
    mkdir "./bin"
fi

gcc -std=c99 -O0 -g -c lib/utils.c -o bin/utils.o
gcc -std=c99 -O0 -g $compiler_flags -pthread -c lib/eviction.c -o bin/eviction.o
gcc -std=c99 -O0 -g -c src/test.c -o bin/test.o
g++ -g -pthread -O0 bin/test.o bin/eviction.o bin/utils.o -o bin/test.out

sudo ./bin/test.out
