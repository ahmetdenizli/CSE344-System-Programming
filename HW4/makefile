
all : hw4.o
	gcc -std=gnu99 hw4.o -o hw4 -lpthread -lm -lrt

hw4.o :
	gcc -c -ansi -pedantic-errors -Wall hw4.c -std=gnu99

clean :
	rm *.o hw4
