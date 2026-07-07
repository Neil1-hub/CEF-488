#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <netinet/tcp.h>
#include "kv_protocol.h"
#include "kv_store.h"

#define MAX_EVENTS 64
#define BUF_SIZE 8192

typedef struct {
    int fd;
    uint8_t buf[BUF_SIZE];
    size_t bytes_read;
} client_session_t;

static volatile int keep_running = 1;
static void handle_sigint(int sig) { (void)sig; keep_running = 0; }

static int make_socket_non_blocking(int sfd) {
    int flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
}

static void configure_socket_options(int fd, int is_tcp) {
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    if (is_tcp) {
        int nodelay = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
        int keepalive = 1;
        setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));
    }
}

static void process_and_reply(int fd, struct sockaddr *cli_addr, socklen_t cli_len, int is_udp, uint8_t *packet_data, size_t packet_len) {
    if (packet_len < sizeof(struct kv_request_header)) return;

    struct kv_request_header req;
    memcpy(&req, packet_data, sizeof(struct kv_request_header));
    deserialize_request_header(&req);

    if (packet_len < req.length) return; // Incomplete packet frame

    // Key is positioned right after the fixed-size binary header
    char *key = (char *)(packet_data + sizeof(struct kv_request_header));
    char *val = NULL;
    
    // If it's a SET operation, locate the value string right after the null-terminated key
    if (req.opcode == OP_SET) {
        size_t key_len = strlen(key) + 1;
        val = key + key_len;
    }

    struct kv_response_header res;
    res.status = STATUS_SUCCESS;
    char reply_payload[BUF_SIZE];
    size_t reply_payload_len = 0;

    if (req.opcode == OP_SET) {
        printf("[Server] Action: SET key='%s' value='%s'\n", key, val);
        if (kv_set(key, val) != 0) res.status = STATUS_ERROR;
    } else if (req.opcode == OP_GET) {
        printf("[Server] Action: GET key='%s'\n", key);
        const char *found_val = kv_get(key);
        if (found_val) {
            reply_payload_len = strlen(found_val) + 1;
            memcpy(reply_payload, found_val, reply_payload_len);
        } else {
            res.status = STATUS_NOT_FOUND;
        }
    } else if (req.opcode == OP_DEL) {
        printf("[Server] Action: DEL key='%s'\n", key);
        if (kv_del(key) != 0) res.status = STATUS_NOT_FOUND;
    } else {
        res.status = STATUS_ERROR;
    }

    res.length = sizeof(struct kv_response_header) + reply_payload_len;
    
    uint8_t send_buf[BUF_SIZE];
    struct kv_response_header wire_res = res;
    serialize_response_header(&wire_res);
    
    memcpy(send_buf, &wire_res, sizeof(struct kv_response_header));
    if (reply_payload_len > 0) {
        memcpy(send_buf + sizeof(struct kv_response_header), reply_payload, reply_payload_len);
    }

    if (is_udp) {
        sendto(fd, send_buf, res.length, 0, cli_addr, cli_len);
    } else {
        send(fd, send_buf, res.length, 0);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    signal(SIGINT, handle_sigint);
    kv_init();

    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, argv[1], &hints, &result) != 0) {
        perror("getaddrinfo TCP failed");
        exit(EXIT_FAILURE);
    }
    
    int listen_fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (listen_fd == -1) continue;
        int v6only = 0;
        setsockopt(listen_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        configure_socket_options(listen_fd, 1);
        if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(listen_fd);
    }
    freeaddrinfo(result);

    if (!rp || listen(listen_fd, 128) == -1) {
        perror("Could not bind/listen TCP");
        exit(EXIT_FAILURE);
    }
    make_socket_non_blocking(listen_fd);

    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(NULL, argv[1], &hints, &result) != 0) {
        perror("getaddrinfo UDP failed");
        exit(EXIT_FAILURE);
    }
    
    int udp_fd = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        udp_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (udp_fd == -1) continue;
        int v6only = 0;
        setsockopt(udp_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        configure_socket_options(udp_fd, 0);
        if (bind(udp_fd, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(udp_fd);
    }
    freeaddrinfo(result);
    if (!rp) {
        fprintf(stderr, "Could not bind UDP\n");
        exit(EXIT_FAILURE);
    }
    make_socket_non_blocking(udp_fd);

    int epoll_fd = epoll_create1(0);
    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    ev.events = EPOLLIN;
    ev.data.fd = udp_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, udp_fd, &ev);

    printf("Dual-stack Server operational on port %s...\n", argv[1]);

    while (keep_running) {
        int nfps = epoll_wait(epoll_fd, events, MAX_EVENTS, 1000);
        for (int i = 0; i < nfps; i++) {
            if (events[i].data.fd == listen_fd) {
                struct sockaddr_in6 cli_addr;
                socklen_t cli_len = sizeof(cli_addr);
                int client_fd = accept(listen_fd, (struct sockaddr*)&cli_addr, &cli_len);
                if (client_fd != -1) {
                    make_socket_non_blocking(client_fd);
                    configure_socket_options(client_fd, 1);
                    client_session_t *session = calloc(1, sizeof(client_session_t));
                    session->fd = client_fd;
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.ptr = session;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                }
            } else if (events[i].data.fd == udp_fd) {
                struct sockaddr_in6 cli_addr;
                socklen_t cli_len = sizeof(cli_addr);
                uint8_t udp_buf[BUF_SIZE];
                ssize_t recvd = recvfrom(udp_fd, udp_buf, BUF_SIZE, 0, (struct sockaddr*)&cli_addr, &cli_len);
                if (recvd >= (ssize_t)sizeof(struct kv_request_header)) {
                    process_and_reply(udp_fd, (struct sockaddr*)&cli_addr, cli_len, 1, udp_buf, recvd);
                }
            } else {
                client_session_t *session = (client_session_t*)events[i].data.ptr;
                int done = 0;
                while (1) {
                    ssize_t count = read(session->fd, session->buf + session->bytes_read, BUF_SIZE - session->bytes_read);
                    if (count > 0) {
                        session->bytes_read += count;
                    } else if (count == 0) {
                        done = 1;
                        break;
                    } else {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        done = 1;
                        break;
                    }
                }
                
                if (session->bytes_read >= sizeof(struct kv_request_header)) {
                    struct kv_request_header test_req;
                    memcpy(&test_req, session->buf, sizeof(struct kv_request_header));
                    deserialize_request_header(&test_req);
                    
                    if (session->bytes_read >= test_req.length) {
                        process_and_reply(session->fd, NULL, 0, 0, session->buf, test_req.length);
                        done = 1;
                    }
                }
                
                if (done) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, session->fd, NULL);
                    close(session->fd);
                    free(session);
                }
            }
        }
    }

    close(listen_fd);
    close(udp_fd);
    close(epoll_fd);
    kv_destroy();
    return 0;
}
