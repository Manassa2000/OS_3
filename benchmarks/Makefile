all : test
test: ../my_vm.h
	gcc bench.c -L../ -lmy_vm -m32 -o bench

clean:
	rm -rf test mtest