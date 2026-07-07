

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define THREAD_COUNT 4
#define QUEUE_SIZE 256

int queue[QUEUE_SIZE];
int queue_head = 0, queue_tail = 0, queue_count = 0;
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        char method[16], path[256], protocol[16];
        sscanf(buffer, "%s %s %s", method, path, protocol);

        if (strstr(path, "..") != NULL) {
            char *res = "HTTP/1.0 403 Forbidden\r\nContent-Length: 9\r\n\r\nForbidden";
            send(client_fd, res, strlen(res), 0);
        } else {
            char actual_path[512];
            if (strcmp(path, "/") == 0) strcpy(path, "/index.html");
            snprintf(actual_path, sizeof(actual_path), "./wwwroot%s", path);

            int file_fd = open(actual_path, O_RDONLY);
            if (file_fd < 0) {
                char *not_found = "HTTP/1.0 404 Not Found\r\nContent-Length: 9\r\n\r\nNot Found";
                send(client_fd, not_found, strlen(not_found), 0);
            } else {
                struct stat file_stat;
                fstat(file_fd, &file_stat);
                char header[256];
                int header_len = snprintf(header, sizeof(header),
                    "HTTP/1.0 200 OK\r\nContent-Length: %lld\r\nContent-Type: text/html\r\n\r\n", (long long)file_stat.st_size);
                send(client_fd, header, header_len, 0);

                char file_buf[BUFFER_SIZE];
                ssize_t file_bytes;
                while ((file_bytes = read(file_fd, file_buf, sizeof(file_buf))) > 0) {
                    send(client_fd, file_buf, file_bytes, 0);
                }
                close(file_fd);
            }
        }
    }
    close(client_fd);
}

void *worker_thread() {
    while (1) {
        int client_fd = -1;
        pthread_mutex_lock(&queue_mutex);
        while (queue_count == 0) {
            pthread_cond_wait(&queue_cond, &queue_mutex);
        }
        client_fd = queue[queue_head];
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        queue_count--;
        pthread_mutex_unlock(&queue_mutex);

        if (client_fd != -1) {
            handle_client(client_fd);
        }
    }
    return NULL;
}

int main() {
    pthread_t pool[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        pthread_create(&pool[i], NULL, worker_thread, NULL);
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY
    };

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 128);
    printf("Thread Pool Server running with %d workers on port %d...\n", THREAD_COUNT, PORT);

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;

        pthread_mutex_lock(&queue_mutex);
        if (queue_count < QUEUE_SIZE) {
            queue[queue_tail] = client_fd;
            queue_tail = (queue_tail + 1) % QUEUE_SIZE;
            queue_count++;
            pthread_cond_signal(&queue_cond);
        } else {
            close(client_fd);
        }
        pthread_mutex_unlock(&queue_mutex);
    }
    return 0;
}
