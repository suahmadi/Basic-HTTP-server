CXX = g++ -fPIC
NETLIBS= -lnsl
CC= gcc

all: myhttpd use-dlopen hello.so jj-mod util jj-mod.so



jj-mod: jj-mod.c
	$(CC) -c -fPIC -o $@.o jj-mod.c

util: util.c
	$(CC) -c -fPIC -o $@.o util.c

jj-mod.so: jj-mod.o util.o
	ld -G -o jj-mod.so jj-mod.o util.o

myhttpd : myhttpd.o
	$(CXX) -pthread -o $@ $@.o $(NETLIBS) -ldl -lpthread

use-dlopen: use-dlopen.o
	$(CXX) -o $@ $@.o $(NETLIBS) -ldl

hello.so: hello.o
	ld -G -o hello.so hello.o

%.o: %.cc
	@echo 'Building $@ from $<'
	$(CXX) -o $@ -c -I. $<



.PHONY: clean
clean:
	rm -f *.o use-dlopen hello.so
	rm -f *.o myhttpd

