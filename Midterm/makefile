
all : mid.o
	gcc -std=gnu99 client.o output.o -o client
	gcc -std=gnu99 serverY.o output.o -o serverY  -lpthread -lm -lrt

mid.o :
	gcc -c -ansi -pedantic-errors -Wall client.c -std=gnu99
	gcc -c -ansi -pedantic-errors -Wall serverY.c -std=gnu99
	gcc -c -ansi -pedantic-errors -Wall output.c -std=gnu99
		
clean :
	rm *.o serverY client
