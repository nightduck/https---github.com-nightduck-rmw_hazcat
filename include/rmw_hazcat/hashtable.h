#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <sys/types.h>

typedef struct node {
    node_t * next;
    int id;
    void * addr;
} node_t;

typedef struct hashtable {
    size_t len;
    node_t[len] table;
} hashtable_t;

#define HASH_TABLE(size)    node_t[size]
#define HASH(val, size)    val % size

inline void * hash_get(node_t[] table, int id) {
    node_t * it = &table[HASH(id, size)]
}

#endif  //_HASHTABLE_H