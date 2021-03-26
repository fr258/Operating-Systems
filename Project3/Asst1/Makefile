memgrind: memgrind.o mymalloc.o
	gcc -Wall memgrind.o mymalloc.o -o memgrind

memgrind.o: memgrind.c mymalloc.h
	gcc -Wall -c memgrind.c

mymalloc.o: mymalloc.c mymalloc.h
	gcc -Wall -c mymalloc.c

clean:
	rm -f memgrind *.o
