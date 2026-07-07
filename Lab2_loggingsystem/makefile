CC = gcc
CFLAGS = -Wall -Wextra

all: server client control

server: log_server.c
	$(CC) $(CFLAGS) -o server log_server.c

client: log_client.c
	$(CC) $(CFLAGS) -o client log_client.c

control: control_client.c
	$(CC) $(CFLAGS) -o control control_client.c

clean:
	rm -f server client control
