CC=gcc
CFLAGS=-c -O2 -Wall -Wextra -Werror
LFLAGS=-lm -pthread

all:	problem_7.o
	$(CC) problem_7.o -o problem_7 $(LFLAGS)

problem_7.o:
	$(CC) $(CFLAGS) problem_7.c
 
clean:
	rm -rf *o problem_7
