#ifndef KV_PROTOCOL_H
#define KV_PROTOCOL_H

#include <stdint.h>
#include <arpa/inet.h>

#define OP_SET         1
#define OP_GET         2
#define OP_DEL         3

#define STATUS_SUCCESS   0
#define STATUS_NOT_FOUND 1
#define STATUS_ERROR     2

// Request fixed-size header (Total packet = length)
struct kv_request_header {
    uint32_t length; // Includes header length + variable key/value sizes
    uint32_t opcode; // OP_SET, OP_GET, OP_DEL
} __attribute__((packed));

// Response fixed-size header
struct kv_response_header {
    uint32_t length; // Includes header length + variable payload data
    uint32_t status; // STATUS_SUCCESS, STATUS_NOT_FOUND, STATUS_ERROR
} __attribute__((packed));

// Helper inline functions to ensure host-to-network translations
static inline void serialize_request_header(struct kv_request_header *req) {
    req->length = htonl(req->length);
    req->opcode = htonl(req->opcode);
}

static inline void deserialize_request_header(struct kv_request_header *req) {
    req->length = ntohl(req->length);
    req->opcode = ntohl(req->opcode);
}

static inline void serialize_response_header(struct kv_response_header *res) {
    res->length = htonl(res->length);
    res->status = htonl(res->status);
}

static inline void deserialize_response_header(struct kv_response_header *res) {
    res->length = ntohl(res->length);
    res->status = ntohl(res->status);
}

#endif // KV_PROTOCOL_H
