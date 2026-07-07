#include "kv_store.h"
#include <stdlib.h>
#include <string.h>

static kv_node_t *hash_table[HASH_TABLE_SIZE];

// Simple Jenkins One-at-a-time hash function
static unsigned int hash(const char *key) {
    unsigned int hash = 0;
    while (*key) {
        hash += *key++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash % HASH_TABLE_SIZE;
}

void kv_init(void) {
    memset(hash_table, 0, sizeof(hash_table));
}

int kv_set(const char *key, const char *value) {
    unsigned int idx = hash(key);
    kv_node_t *curr = hash_table[idx];
    
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            free(curr->value);
            curr->value = strdup(value);
            return 0;
        }
        curr = curr->next;
    }
    
    kv_node_t *new_node = malloc(sizeof(kv_node_t));
    if (!new_node) return -1;
    new_node->key = strdup(key);
    new_node->value = strdup(value);
    new_node->next = hash_table[idx];
    hash_table[idx] = new_node;
    return 0;
}

const char* kv_get(const char *key) {
    unsigned int idx = hash(key);
    kv_node_t *curr = hash_table[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) return curr->value;
        curr = curr->next;
    }
    return NULL;
}

int kv_del(const char *key) {
    unsigned int idx = hash(key);
    kv_node_t *curr = hash_table[idx];
    kv_node_t *prev = NULL;
    
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            if (prev) prev->next = curr->next;
            else hash_table[idx] = curr->next;
            free(curr->key);
            free(curr->value);
            free(curr);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

void kv_destroy(void) {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        kv_node_t *curr = hash_table[i];
        while (curr) {
            kv_node_t *tmp = curr;
            curr = curr->next;
            free(tmp->key);
            free(tmp->value);
            free(tmp);
        }
    }
}
