
all : final.o
	gcc -std=gnu99 server.o utilities.o queue.o -o server -pthread -lrt
	gcc -std=gnu99 servant.o utilities.o  queue.o  LinkedList.o -o servant -pthread -lrt
	gcc -std=gnu99 client.o utilities.o -o client -pthread -lrt

final.o :
	gcc -c server.c -std=gnu99 -pedantic -Wall
	gcc -c servant.c -std=gnu99 -pedantic -Wall
	gcc -c client.c -std=gnu99 -pedantic -Wall
	gcc -c LinkedList.c -std=gnu99 -pedantic -Wall
	gcc -c queue.c -std=gnu99 -pedantic -Wall
	gcc -c utilities.c -std=gnu99 -pedantic -Wall

clean :
	rm *.o server servant client
