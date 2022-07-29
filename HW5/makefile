
all : hw5.o
	gcc -std=gnu99 hw5.o -o hw5 -lpthread -lm -lrt

hw5.o :
	gcc -c -ansi -pedantic-errors -Wall hw5.c -std=gnu99

clean :
	rm *.o hw5
