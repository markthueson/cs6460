CC=gcc
CFLAGS=-c -O2 -Wall -Wextra -Werror

all:	dev_access.o
	$(CC) dev_access.o -o dev_access

dev_access.o:
	$(CC) $(CFLAGS) dev_access.c
 
clean:
	rm -rf *o dev_access
