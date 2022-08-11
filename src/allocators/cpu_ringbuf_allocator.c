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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdatomic.h>
#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"

cpu_ringbuf_allocator_t * create_cpu_ringbuf_allocator(size_t item_size, size_t ring_size)
{
  cpu_ringbuf_allocator_t * alloc = (cpu_ringbuf_allocator_t *)create_shared_allocator(
    NULL, sizeof(cpu_ringbuf_allocator_t) + (item_size + sizeof(atomic_int)) * ring_size,
    0, LOCAL_GRANULARITY, ALLOC_RING, CPU, 0);

  if (alloc == NULL) {
    return NULL;
  }

  alloc->count = 0;
  alloc->rear_it = 0;
  alloc->item_size = item_size;
  alloc->ring_size = ring_size;

  return alloc;
}

int cpu_ringbuf_allocate(void * self, size_t size)
{
  cpu_ringbuf_allocator_t * s = (cpu_ringbuf_allocator_t *)self;
  if (s->count == s->ring_size) {
    // Allocator full
    return -1;
  }
  int forward_it = (s->rear_it + s->count) % s->ring_size;

  // Give address relative to allocator, taking into account the 4 bytes in front for ref counter
  int ret = sizeof(cpu_ringbuf_allocator_t) + sizeof(atomic_int) +
    (s->item_size + sizeof(atomic_int)) * forward_it;

  // Set reference counter to 1
  atomic_int * ref_count = (atomic_int *)(self + ret - sizeof(atomic_int));
  atomic_store(ref_count, 1);

  // Update count of how many elements in pool
  s->count++;

  return ret;
}

void cpu_ringbuf_share(void * self, int offset)
{
  atomic_int * ref_count = (uint8_t *)self + offset - sizeof(atomic_int);
  atomic_fetch_add(ref_count, 1);
}

void cpu_ringbuf_deallocate(void * self, int offset)
{
  cpu_ringbuf_allocator_t * s = (cpu_ringbuf_allocator_t *)self;
  if (s->count == 0) {
    return;       // Allocator empty, nothing to deallocate
  }

  // Decrement reference counter and only go through with deallocate if it's zero
  // (will read as 1 because fetch happens before decrement)
  atomic_int * ref_count = (uint8_t *)self + offset - sizeof(atomic_int);
  if (atomic_fetch_add(ref_count, -1) > 1) {
    return;
  }

  int entry = (offset - sizeof(cpu_ringbuf_allocator_t)) /
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
  size_t dev_size = 0x80000;
  void * dev_gran = 0x10000;

  // Get size of shared memory
  struct shmid_ds buf;
  if (shmctl(temp->shmem_id, IPC_STAT, &buf) == -1) {
    printf("cpu_ringbuf_remap failed on fetching segment info\n");
    handle_error("shmctl");
    return;
  }

  // Reserve a memory range for local and shared portions (no device pool needed)
  void * mapping = reserve_memory_for_allocator(buf.shm_segsz, 0, LOCAL_GRANULARITY);
  if (mapping == MAP_FAILED) {
    return NULL;
  }

  // Map in local portion
  void * local = mmap(
    mapping, LOCAL_GRANULARITY,
    PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (local == MAP_FAILED) {
    printf("cpu_ringbuf_remap failed on creation of local portion\n");
    handle_error("mmap");
  }

  // Map in shared portion of allocator
  void * shared = shmat(temp->shmem_id, mapping + LOCAL_GRANULARITY, SHM_REMAP);
  if (shared == MAP_FAILED) {
    printf("cpu_ringbuf_remap failed on creation of shared portion\n");
    handle_error("shmat");
  }

  hma_allocator_t * alloc = (hma_allocator_t *)(shared - sizeof(fps_t));
  populate_local_fn_pointers(alloc, temp->device_type << 12 | temp->strategy);

  // Returned pointer is in unmapped memory
  return alloc;
}

void cpu_ringbuf_unmap(struct hma_allocator * alloc)
{
  // Unmap self, and destroy segment, if this is the last one
  struct shmid_ds buf;
  if (shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    // return RMW_RET_ERROR;
    return;
  }
  if (buf.shm_cpid == getpid()) {
    printf("Marking segment fo removal\n");
    if (shmctl(alloc->shmem_id, IPC_RMID, NULL) == -1) {
      printf("Destruction failed on marking segment for removal\n");
      // RMW_SET_ERROR_MSG("can't mark shared StaticPoolAllocator for deletion");
      // return RMW_RET_ERROR;
      return;
    }
  }
  void * shared_portion = (void *)((uint8_t *)alloc + sizeof(fps_t));
  int ret = shmdt(shared_portion);
  if (ret) {
    printf("unmap_shared_allocator, failed to detach\n");
    handle_error("shmdt");
  }

  // Remove local mapping
  if (munmap((uint8_t *)alloc + sizeof(fps_t) - LOCAL_GRANULARITY, LOCAL_GRANULARITY)) {
    printf("failed to detach local portion of allocator\n");
    handle_error("munmap");
  }
}

#ifdef __cplusplus
}
#endif
