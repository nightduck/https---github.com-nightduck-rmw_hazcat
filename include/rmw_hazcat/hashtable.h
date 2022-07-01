#ifndef _HASHTABLE_H
#define _HASHTABLE_H

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

// NOTE: This implementation is not thread safe, and operations are not atomic

typedef struct node
{
  node_t* next;
  int key;
  void* val;
} node_t;

typedef struct hashtable
{
  size_t len;
  size_t count;
  node_t* table;
} hashtable_t;

#define HASH(val, size) val ^ 0xDEADBEEF % size

hashtable_t* hashtable_init(size_t len)
{
  hashtable_t* table = (hashtable_t*)malloc(sizeof(hashtable_t) + len * sizeof(node_t));
  table->len = len;
  table->count = 0;
  table->table = (node_t*)((uint8_t*)table + sizeof(hashtable_t));
  for (int i = 0; i < len; i++)
  {
    table->table[i].next = &table->table[i];  // Terminating nodes point at themselvess
    table->table[i].val = nullptr;
  }
  return table;
}

void hashtable_fini(hashtable_t* ht)
{
  free(ht);
}

void hashtable_insert(hashtable* ht, int key, void* val)
{
  if (ht->count >= ht->len)
  {
    return;  // Table full
  }

  // Case 1 - Hash is free: both loops skipped, tail and it equal, so only last 4 lines matter
  // Case 2 - Hash is in-use: first loop iterates around table through a linked list of other
  //          collisions, terminating on a occupied spot. 2nd loop iterates from that point
  //          in the table until an empty spot is found. It is then linked to the last element in
  //          the collision list

  node_t* it = &ht->table[HASH(key, ht->len)];

  while (it->next != nullptr)                   // This bucket is occupied, traverse down the list
  {
    it = it->next;
  }
  node_t* tail = it;
  while (it->val != nullptr)                      // Now find a free index to link to
  {
    it++;
    it = ht->table + (it - ht->table) % ht->len;  // Wrap around
  }
  tail->next = it;
  it->next = nullptr;
  it->key = key;
  it->val = val;
  ht->count++;
}

void* hashtable_get(hashtable_t* ht, int key) {
  node_t* it = &ht->table[HASH(key, ht->len)];
  while(it != nullptr && it->key != key) {
    it = it->next;
  }
  return it->val;    // Return either a match or a null pointer, whatever we stumble upon
}

void hashtable_remove(hashtable* ht, int key)
{
  node_t dummy; // Temporary structure to act as a dummy head
  dummy.next = &ht->table[HASH(key, ht->len)];    // Have it point to the actual head

  node_t * it = &dummy;

  while(it->next != nullptr && it->next->key != key) {
    it = it->next;
  }
  if(it->next != nullptr) {   // If we've reached an element that matches the key 
    node_t * removing = it->next;
    it->next = removing->next;
    removing->next = nullptr;
    removing->val = nullptr;
  }
  // Otherwise there's nothing to remove
}

#endif  //_HASHTABLE_H