// Copyright 2022 Washington University in St Louis

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdatomic.h>
#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"

struct cpu_ringbuf_allocator * create_cpu_ringbuf_allocator(size_t item_size, size_t ring_size)
{
  struct cpu_ringbuf_allocator * alloc = (struct cpu_ringbuf_allocator *)create_shared_allocator(
    sizeof(struct cpu_ringbuf_allocator) + (item_size + sizeof(atomic_int)) * ring_size,
    0, LOCAL_GRANULARITY, ALLOC_RING, CPU, 0);

  alloc->count = 0;
  alloc->rear_it = 0;
  alloc->item_size = item_size;
  alloc->ring_size = ring_size;

  return alloc;
}

int cpu_ringbuf_allocate(void * self, size_t size)
{
  struct cpu_ringbuf_allocator * s = (struct cpu_ringbuf_allocator *)self;
  if (s->count == s->ring_size) {
    // Allocator full
    return -1;
  }
  int forward_it = (s->rear_it + s->count) % s->ring_size;

  // Give address relative to allocator, taking into account the 4 bytes in front for reference counter
  int ret = sizeof(struct cpu_ringbuf_allocator) + sizeof(atomic_int) + (s->item_size + sizeof(atomic_int)) * forward_it;

  // Set reference counter to 1
  atomic_int * ref_count = (atomic_int*)(self + ret - sizeof(atomic_int));
  atomic_store(ref_count, 1);

  // Update count of how many elements in pool
  s->count++;

  return ret;
}

void cpu_ringbuf_share(void * self, int offset) {
  atomic_int * ref_count = (uint8_t*)self + offset - sizeof(atomic_int);
  atomic_fetch_add(ref_count, 1);
}

void cpu_ringbuf_deallocate(void * self, int offset)
{
  struct cpu_ringbuf_allocator * s = (struct cpu_ringbuf_allocator *)self;
  if (s->count == 0) {
    return;       // Allocator empty, nothing to deallocate
  }

  // Decrement reference counter and only go through with deallocate if it's zero 
  // (will read as 1 because fetch happens before decrement)
  atomic_int * ref_count = (uint8_t*)self + offset - sizeof(atomic_int);
  if(atomic_fetch_add(ref_count, -1) > 1) {
    return;
  }

  int entry = (offset - sizeof(struct cpu_ringbuf_allocator)) /
    (s->item_size + sizeof(atomic_int));

  // Do math with imaginary overflow indices so forward_it >= entry >= rear_it
  int forward_it = s->rear_it + s->count;
  if (__glibc_unlikely(entry < s->rear_it)) {
    entry += s->ring_size;
  }

  if (__glibc_unlikely(entry >= forward_it)) {
    // Invalid argument, already deallocated
    return;
  }

  // Most likely scenario: entry == rear_it as allocations are deallocated in order
  s->rear_it = entry + 1;
  s->count = forward_it - s->rear_it;
  s->rear_it %= s->ring_size;
}

struct hma_allocator * cpu_ringbuf_remap(struct hma_allocator * temp)
{
  // Map in shared portion of allocator
  void * shared = shmat(temp->shmem_id, NULL, 0);
  if (shared == MAP_FAILED) {
    printf("cpu_ringbuf_remap failed on creation of shared portion\n");
    handle_error("shmat");
  }

  // Returned pointer is in unmapped memory
  return (hma_allocator_t*)(shared - sizeof(fps_t));
}

void cpu_ringbuf_unmap(struct hma_allocator * alloc)
{
  // No device pool to remove, so just return
  return;
}

#ifdef __cplusplus
}
#endif
