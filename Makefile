all:	hash.o server
	gcc hash.o WTF.c -o WTF -lcrypto -lssl
server:	hash.o
	gcc hash.o WTFServer.c -o WTFServer -lcrypto -lssl
hash.o:	hash.c
	gcc -c hash.c
clean:
	rm hash.o;rm WTF; rm WTFServer
