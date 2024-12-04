default: lib/eviction.c lib/eviction.h lib/utils.c lib/utils.h
	mkdir -p "./bin"

	gcc -std=c99 -O0 -g -pthread -c lib/eviction.c -o bin/eviction.o
	gcc -std=c99 -O0 -g -c lib/utils.c -o bin/utils.o

clean: bin/eviction.o bin/utils.o
	rm bin/eviction.o bin/utils.o
