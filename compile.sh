#!/bin/bash
if [ ! -d "./bin" ]; then
    mkdir "./bin"
fi

gcc -std=c99 -O0 -g -c lib/utils.c -o bin/utils.o
gcc -std=c99 -O0 -g $compiler_flags -pthread -c lib/eviction.c -o bin/eviction.o
gcc -std=c99 -O0 -g -c lib/covert.c -o bin/covert.o
gcc -std=c99 -O0 -g -c src/test.c -o bin/test.o
gcc -std=c99 -O0 -g -c src/victim.c -o bin/victim.o
g++ -g -pthread -O0 bin/test.o bin/covert.o bin/eviction.o bin/utils.o -o bin/test.out
g++ -g -pthread -O0 bin/victim.o bin/covert.o bin/eviction.o bin/utils.o -o bin/victim.out


