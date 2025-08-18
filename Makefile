CFLAGS := -std=c99

all: server client

server: ./src/server.c ./src/db/sqlitedb.c ./src/yyjson.c ./src/network_utils.c
	gcc $(CFLAGS) -o server ./src/server.c ./src/yyjson.c ./src/network_utils.c ./src/db/sqlitedb.c -lsqlite3

client: ./src/client.c ./src/network_utils.c ./src/yyjson.c ./src/gui.c
	gcc $(CFLAGS) -o client ./src/client.c ./src/yyjson.c ./src/gui.c ./src/network_utils.c `pkg-config --cflags --libs gtk+-3.0`

clean:
	rm -f server client
