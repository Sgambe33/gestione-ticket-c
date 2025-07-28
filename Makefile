all: server client

server: ./src/server.c ./src/db/sqlitedb.c
	gcc -o server ./src/server.c ./src/db/sqlitedb.c -lsqlite3

client: ./src/client.c ./src/gui_utils.c
	gcc -o client ./src/gui_utils.c ./src/client.c `pkg-config --cflags --libs gtk+-3.0`

clean:
	rm -f server client
