#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]){
	if (argc < 2) return 1;
	int fd = open("/tmp/control", O_WRONLY);
	char cmd[100];
	sprintf(cmd, "ADD %s", argv[1]);
	write(fd, cmd, strlen(cmd));
	close(fd);
	return 0;
}

