#ifndef KV_STORE_H
#define KV_STORE_H

#define HASH_TABLE_SIZE 1024

typedef struct kv_node {
    char *key;
    char *value;
    struct kv_node *next;
} kv_node_t;

void kv_init(void);
int kv_set(const char *key, const char *value);
const char* kv_get(const char *key);
int kv_del(const char *key);
void kv_destroy(void);

#endif // KV_STORE_H
