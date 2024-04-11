CC = gcc
CFLAGS = -g -c 
AR = ar -rc
RANLIB = ranlib

my_vm.a: my_vm.o
	$(CC) $(CFLAGS) $^ -o $@     # Compile my_vm.c to object files
	$(AR) $@ $^                  # Create the archive file my_vm.a
	$(RANLIB) $@                 # Generate index for the archive file

test: test.c my_vm.o
	$(CC) $^ -o $@

clean:
	rm -rf *.o *.a


# CC = gcc
# CFLAGS = -g -c 
# AR = ar -rc
# RANLIB = ranlib

# my_vm.a: my_vm.c
# 	$(AR) $@ $^
# 	$(RANLIB) $@

# test: test.c my_vm.a
# 	$(CC) test.c my_vm.a -o test.o

# clean:
# 	rm -rf *.o *.a
