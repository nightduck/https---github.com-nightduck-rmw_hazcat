// Copyright 2022 Washington University in St Louis
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RMW_HAZCAT__HASHTABLE_H_
#define RMW_HAZCAT__HASHTABLE_H_

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

// NOTE: This implementation is not thread safe, and operations are not atomic

typedef struct node
{
  struct node * next;
  int key;
  void * val;
} node_t;

typedef struct hashtable
{
  size_t len;
  size_t count;
  node_t * table;
} hashtable_t;

#define HASH(val, size) (val ^ 0xDEADBEEF) % size

hashtable_t * hashtable_init(size_t len)
{
  hashtable_t * table = (hashtable_t *)malloc(sizeof(hashtable_t) + len * sizeof(node_t));
  table->len = len;
  table->count = 0;
  table->table = (node_t *)((uint8_t *)table + sizeof(hashtable_t));
  for (size_t i = 0; i < len; i++) {
    table->table[i].next = NULL;
    table->table[i].val = NULL;
    table->table[i].key = 0;
  }
  return table;
}

void hashtable_fini(hashtable_t * ht)
{
  free(ht);
}

void hashtable_insert(hashtable_t * ht, int key, void * val)
{
  if (ht->count >= ht->len) {
    return;  // Table full
  }
  if (val == NULL) {
    return;  // Can't insert null pointers
  }

  // Case 1 - Hash is free: both loops skipped, tail and it equal, so only last 4 lines matter
  // Case 2 - Hash is in-use: first loop iterates around table through a linked list of other
  //          collisions, terminating on a occupied spot. 2nd loop iterates from that point
  //          in the table until an empty spot is found. It is then linked to the last element in
  //          the collision list
  // Case 3 - Hash is in-use by an element that doesn't match our hash. Enter special case if block
  //          to move the squatter to a new place, update their list, and then ours is guaranteed to
  //          place without collision
  // Case 4 - Key already in table: Iterate in first loop until landed on entry. Execute special
  //          condition

  node_t * it = &(ht->table[HASH(key, ht->len)]);

  // If space occupied, but no hash collision, move it so we don't mix different hashes in the llist
  if (it->val != NULL && HASH(it->key, ht->len) != HASH(key, ht->len)) {
    // Find the start of the list colliding with us
    node_t dummy;
    node_t * head = &dummy;
    head->next = &(ht->table[HASH(it->key, ht->len)]);

    // Find the element pointing to our desired entry
    while (head->next != it) {
      head = head->next;
    }

    // Find a new spot for the old guy to go
    node_t * squater = head->next;
    while (NULL != squater->val) {                   // Now find a free index to link to
      squater++;
      squater = ht->table + (squater - ht->table) % ht->len;  // Wrap around
    }

    // Move squatter to new home, update their list
    squater->val = it->val;
    squater->key = it->key;
    squater->next = it->next;
    head->next = squater;

    // Put our stuff in
    it->next = NULL;
    it->key = key;
    it->val = val;
    ht->count++;
    return;
  }

  while (NULL != it->next && it->key != key) {  // This bucket is occupied, traverse down the list
    it = it->next;
  }

  // We found another entry with the same key. Update value but leave hashtable unchanged
  if (it->key == key) {
    it->val = val;
    return;
  }

  // At this point, it is either at it's initial value, which is unoccupied, or at the end of a
  // linked list, which is occupied. Assume the latter and iterate over array for empty spot
  node_t * tail = it;
  while (NULL != it->val) {                   // Now find a free index to link to
    it++;
    it = ht->table + (it - ht->table) % ht->len;  // Wrap around
  }

  // Found an empty entry, so populate it
  tail->next = it;  // If Case 1, tail = it, and this line does nothing
  it->next = NULL;
  it->key = key;
  it->val = val;
  ht->count++;
}

void * hashtable_get(hashtable_t * ht, int key)
{
  node_t * it = &(ht->table[HASH(key, ht->len)]);
  while (NULL != it && it->key != key) {
    it = it->next;
  }
  return (it == NULL) ? it : it->val;    // Return either a match or a null pointer
}

void hashtable_remove(hashtable_t * ht, int key)
{
  node_t * front = &(ht->table[HASH(key, ht->len)]);

  // Special case: removing single item
  if (front->key == key && front->next == NULL) {
    front->next = NULL;
    front->val = NULL;
  } else if (front->key == key) {
    // Special case: removing head of list, need to copy second item down
    node_t * second = front->next;
    front->next = second->next;
    front->key = second->key;
    front->val = second->val;
    second->next = NULL;
    second->val = NULL;
  } else {
    // Otherwise iterate through the list
    while (NULL != front->next && front->next->key != key) {
      front = front->next;
    }
    if (NULL != front->next && front->next->key == key) {   // If we've reached an element that
      node_t * removing = front->next;                      // matches the key
      front->next = removing->next;
      removing->next = NULL;
      removing->val = NULL;
    }
  }
  // Otherwise there's nothing to remove
}

#endif  // RMW_HAZCAT__HASHTABLE_H_
