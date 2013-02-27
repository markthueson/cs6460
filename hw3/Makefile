CC=gcc
CFLAGS=-c -O2 -Wall -Wextra -Werror
LFLAGS=-lm -pthread

all:	problem_1.o
	$(CC) problem_1.o -o problem_1 $(LFLAGS)

problem_1.o:
	$(CC) $(CFLAGS) problem_1.c
 
clean:
	rm -rf *o problem_1
