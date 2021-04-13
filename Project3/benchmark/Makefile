
all : test
test: ../my_vm.h
	gcc test.c -g -L../ -lmy_vm -m32 -o test -lm

clean:
	rm -rf test
