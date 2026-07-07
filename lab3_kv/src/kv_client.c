#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <errno.h>
#include "kv_protocol.h"

#define INITIAL_TIMEOUT_MS 200
#define MAX_RETRIES 3
#define BUF_SIZE 4096

void parse_and_print_response(uint8_t *recv_buf, ssize_t len) {
    if (len < (ssize_t)sizeof(struct kv_response_header)) {
        fprintf(stderr, "[Client Error] Corrupted response framing received.\n");
        return;
    }
    struct kv_response_header res;
    memcpy(&res, recv_buf, sizeof(res));
    deserialize_response_header(&res);

    if (res.status == STATUS_SUCCESS) {
        printf("STATUS: SUCCESS\n");
        if (len > (ssize_t)sizeof(struct kv_response_header)) {
            char *val = (char *)(recv_buf + sizeof(struct kv_response_header));
            printf("VALUE:  %s\n", val);
        }
    } else if (res.status == STATUS_NOT_FOUND) {
        printf("STATUS: NOT FOUND\n");
    } else {
        printf("STATUS: KEY-STORE ERROR\n");
    }
}

int execute_udp_transaction(struct addrinfo *res, uint8_t *packet, size_t total_len) {
    int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sockfd == -1) return -1;

    int retry = 0;
    long timeout_ms = INITIAL_TIMEOUT_MS;
    uint8_t recv_buf[BUF_SIZE];

    while (retry <= MAX_RETRIES) {
        sendto(sockfd, packet, total_len, 0, res->ai_addr, res->ai_addrlen);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        ssize_t recvd = recvfrom(sockfd, recv_buf, sizeof(recv_buf), 0, NULL, NULL);
        if (recvd >= 0) {
            parse_and_print_response(recv_buf, recvd);
            close(sockfd);
            return 0;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("[Warn] Packet timeout. Backing off and retrying... (%ld ms)\n", timeout_ms);
            timeout_ms *= 2;
            retry++;
        } else {
            break;
        }
    }

    fprintf(stderr, "[Error] Transaction dropped after maximum retries.\n");
    close(sockfd);
    return -1;
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <host> <port> <-t|-u> <SET|GET|DEL> [key] [val]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *host = argv[1];
    char *port = argv[2];
    int use_udp = (strcmp(argv[3], "-u") == 0);
    char *op = argv[4];

    if ((strcmp(op, "SET") == 0 && argc < 7) || ((strcmp(op, "GET") == 0 || strcmp(op, "DEL") == 0) && argc < 6)) {
        fprintf(stderr, "[Error] Missing Key or Value parameters for specified operation.\n");
        exit(EXIT_FAILURE);
    }

    char *key = argv[5];
    char *val = (strcmp(op, "SET") == 0) ? argv[6] : "";

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = use_udp ? SOCK_DGRAM : SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &res) != 0) {
        perror("Name resolution runtime failure");
        exit(EXIT_FAILURE);
    }

    uint8_t payload_buffer[BUF_SIZE];
    struct kv_request_header req;
    req.opcode = (strcmp(op, "SET") == 0) ? OP_SET : (strcmp(op, "GET") == 0) ? OP_GET : OP_DEL;
    
    size_t key_len = strlen(key) + 1;
    size_t val_len = (req.opcode == OP_SET) ? (strlen(val) + 1) : 0;
    req.length = sizeof(struct kv_request_header) + key_len + val_len;

    struct kv_request_header wire_req = req;
    serialize_request_header(&wire_req);
    
    memcpy(payload_buffer, &wire_req, sizeof(struct kv_request_header));
    memcpy(payload_buffer + sizeof(struct kv_request_header), key, key_len);
    if (val_len > 0) {
        memcpy(payload_buffer + sizeof(struct kv_request_header) + key_len, val, val_len);
    }

    if (use_udp) {
        execute_udp_transaction(res, payload_buffer, req.length);
    } else {
        int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (sockfd == -1 || connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
            perror("[TCP Error] Connection failed");
            freeaddrinfo(res);
            exit(EXIT_FAILURE);
        }

        send(sockfd, payload_buffer, req.length, 0);

        uint8_t recv_buf[BUF_SIZE];
        ssize_t recvd = recv(sockfd, recv_buf, sizeof(recv_buf), 0);
        if (recvd > 0) {
            parse_and_print_response(recv_buf, recvd);
        } else {
            fprintf(stderr, "[TCP Error] Remote endpoint closed session prematurely.\n");
        }
        close(sockfd);
    }

    freeaddrinfo(res);
    return 0;
}
