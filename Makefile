all: server client

server: ./src/server.c ./src/db/sqlitedb.c
	gcc -o server ./src/server.c ./src/db/sqlitedb.c -lsqlite3

client: ./src/client.c
	gcc -o client ./src/client.c `pkg-config --cflags --libs gtk+-3.0`

clean:
	rm -f server client
