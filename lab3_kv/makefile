CC = gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
SRC_DIR = src
OBJ_DIR = obj

SERVER_SRC = $(SRC_DIR)/kv_server.c $(SRC_DIR)/kv_store.c
CLIENT_SRC = $(SRC_DIR)/kv_client.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $^ -o server

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) $^ -o client

clean:
	rm -f server client
	rm -rf *.dSYM

.PHONY: all clean
