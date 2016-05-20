## This is a simple Makefile with lost of comments 
## Check Unix Programming Tools handout for more info.

# Define what compiler to use and the flags.
CC=gcc
CXX=g++
CCFLAGS= -g -std=c99 -Wall -Werror

all: shell

# Compile all .c files into .o files
# % matches all (like * in a command)
# $< is the source file (.c file)
%.o : %.c
	$(CC) -c $(CCFLAGS) $<



shell: shell.o 
	$(CC) -o shell shell.o $(CCFLAGS)

clean:
	rm -f core *.o shell