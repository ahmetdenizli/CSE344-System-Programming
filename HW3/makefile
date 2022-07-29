
all : hw3.o
	gcc -std=gnu99 hw3named.o -o hw3named -lpthread -lm -lrt
	gcc -std=gnu99 hw3unnamed.o -o hw3unnamed -lpthread -lm -lrt

hw3.o :
	gcc -c -ansi -pedantic-errors -Wall hw3named.c -std=gnu99
	gcc -c -ansi -pedantic-errors -Wall hw3unnamed.c -std=gnu99

clean :
	rm *.o hw3named hw3unnamed
