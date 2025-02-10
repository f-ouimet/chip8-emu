CC = gcc
all: 
	$(CC) emulator.c -o chip8
clean: 
	rm -f chip8
