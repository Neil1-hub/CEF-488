#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(int argc, char *argv[]){
	if (argc < 3) return 1;
	int fd = open(argv[1], O_WRONLY);
	if (fd == -1) return 1;
//atomic write

	write(fd, argv[2], strlen(argv[2]));
	close(fd);
	return 0;
}
