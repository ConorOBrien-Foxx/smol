FLAGS=-g -Wall
CC=gcc

make:
	$(CC) $(FLAGS) smol.c file.c -o smol