#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define MAX_EVENTS 10
#define BUF_SIZE 4096
#define CONTROL_FIFO "/tmp/control"
#define ALERT_FIFO "/tmp/alert_fifo"

FILE *log_file;
//REAL TIME SIGNAL CONFIGURATION

#define SIG_LOG_NOTIFY (SIGRTMIN + 1)

void rt_signal_handler(int sig, siginfo_t *info, void *context) {
	printf("\n TASK4 RT signal %d: Data ready on FD %d\n", sig, info->si_value.sival_int);
}


// DYNAMIC ADDITION  LOGIC

int epfd, alert_fd;
void add_to_monitoring(int ep_fd, const char *path){
	mkfifo(path, 06660);
	int fd = open(path, O_RDONLY | O_NONBLOCK);
	if (fd == -1) return;

//EDGE TRIGGERED EPOLL
	struct epoll_event ev;
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = fd;
	epoll_ctl(ep_fd, EPOLL_CTL_ADD, fd, &ev);
	
	printf("TASK3: Monitoring started for: %s\n", path);
}


void handle_cleanup(int sig);

int main(){

//SIGNAL SETUP
	signal(SIGINT, handle_cleanup);
	
	struct sigaction sa;
	sa.sa_sigaction = rt_signal_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIG_LOG_NOTIFY, &sa, NULL);

//initial setup for task 1

	mkfifo(ALERT_FIFO, 0666);
	alert_fd = open(ALERT_FIFO, O_RDWR | O_NONBLOCK);
	epfd = epoll_create1(0);
	
	add_to_monitoring(epfd, "/tmp/fifo1");
	add_to_monitoring(epfd, "/tmp/fifo2");
	add_to_monitoring(epfd, "/tmp/fifo3");
	add_to_monitoring(epfd, CONTROL_FIFO);

	struct epoll_event
	events[MAX_EVENTS];
	char buf[BUF_SIZE];


	log_file = fopen("syslog.txt", "a");
	if (log_file == NULL) {
		perror("error opening syslog.txt");
		exit(1);
	}	 
	printf("Server is  runni ng (PID: %d)... \n", getpid());

	while (1){
		int nfds = epoll_wait(epfd, events, MAX_EVENTS, -1);
		for (int i =0; i< nfds; i++){
			int fd = events[i].data.fd;
			ssize_t n;
			while ((n = read(fd, buf,  BUF_SIZE -1)) > 0){
				buf[n] = '\0';

				fprintf(log_file, "LOG: %s\n", buf);
				fflush(log_file);
				printf("Received: %s\n", buf);
//PIPE CONTROL 

				if (strncmp(buf, "ADD ", 4) == 0){
					buf[strcspn(buf, "\n")] = 0;
	add_to_monitoring(epfd, buf + 4);
					continue;
				}


//LOGGING AND ALERTS


				printf("[LOG] FD %d: %s\n", fd, buf);
				if (strstr(buf, "CRITICAL")) {
					write(alert_fd, "ALERT: ", 7);
					write(alert_fd, buf, n);
				}

//NOTIFY VIA A SIGNAL

				union sigval sv;
				sv.sival_int = fd;
				sigqueue(getpid(), SIG_LOG_NOTIFY, sv);
			}
			if (n == 0){
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
			}
		}
	}
	return 0;
}

void handle_cleanup(int sig) {
	printf("\n [CLEANUP] Signal %d received. Cleaning up FIFOs...\n", sig);
	unlink("/tmp/fifo1");
	unlink("/tmp/fifo2");
	unlink("/tmp/fifo3");
	unlink(CONTROL_FIFO);
	unlink(ALERT_FIFO);
	exit(0);
}
