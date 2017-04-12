make: server.c client.c
	gcc -o server server.c
	gcc -o client client.c

clean: server.c client.c
	rm -f client
	rm -f server
	gcc -o server server.c
	gcc -o client client.c
