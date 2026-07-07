
CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

TARGETS = http_basic http_fork http_threadpool http_epoll

all: $(TARGETS)

http_basic: http_basic.c
	$(CC) $(CFLAGS) -o http_basic http_basic.c

http_fork: http_fork.c
	$(CC) $(CFLAGS) -o http_fork http_fork.c

http_threadpool: http_threadpool.c
	$(CC) $(CFLAGS) -o http_threadpool http_threadpool.c

http_epoll: http_epoll.c
	$(CC) $(CFLAGS) -o http_epoll http_epoll.c

clean:
	rm -f $(TARGETS)
