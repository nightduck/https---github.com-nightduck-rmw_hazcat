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

#ifndef HMA_ALLOCATOR_H
#define HMA_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define _GNU_SOURCE

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <unistd.h>

#define OFFSET_TO_PTR(a, o, type) (type*)(uint8_t *)a + o
#define PTR_TO_OFFSET(a, p) (uint8_t *)p - (uint8_t *)a

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_POOL_SIZE   0x100000000

#define ALLOC_RING      0x000
#define ALLOC_TLSF      0x001
#define ALLOC_BEST_FIT  0x002
#define ALLOC_FIRST_FIT 0x003
#define ALLOC_HALF_FIT  0x004
#define ALLOC_STRAT     0x005   // Not for use, indicates max
#define NUM_STRATS      0x1

#define CPU             0x000
#define CUDA            0x001
#define DEVICE          0x002   // Not for use, indicates max
#define NUM_DEV_TYPES   0x2

#define LOCAL_GRANULARITY   (__getpagesize ())
#define SHARED_GRANULARITY  SHMLBA

#define LOCAL_ALIGNMENT(a, t)   (uint8_t*)a - ((uint8_t*)a % LOCAL_GRANULARITY)
#define SHARED_ALIGNMENT(a, t)  (uint8_t*)a + sizeof(fps_t)
//BROKEN#define DEVICE_ALIGNMENT(a, t)  (uint8_t*)(((long)a + sizeof(fps_t)) / SHARED_GRANULARITY * SHARED_GRANULARITY + SHARED_GRANULARITY)

/*
  // Copy paste at head of new allocators, so first 34 bytes can be cast as an hma_allocator
  union {
    struct
    {
      fps_t fps;
      const int shmem_id;
      const uint16_t device_type;
      const uint16_t device_number;
      const uint16_t strategy;
    };
    struct hma_allocator untyped;
  };
*/

typedef struct hma_allocator
{
  int  (* allocate)   (void *, size_t);
  void (* share)      (void *, int);
  void (* deallocate) (void *, int);
  void (* copy_from)  (void *, void *, size_t);
  void (* copy_to)    (void *, void *, size_t);
  void (* copy)       (void *, void *, size_t, struct hma_allocator *);

  //----Boundary between local memory and shared memory mapping occurs here---

  int shmem_id;
  union {   // Only allocators in same domain (same device) can use each other's memory
    struct {
      uint16_t device_type;
      uint16_t device_number;
    };
    uint32_t domain;
  };
  uint16_t strategy;
} hma_allocator_t;

typedef struct function_pointers {
  int  (* allocate)   (void *, size_t);
  void (* share)      (void *, int);
  void (* deallocate) (void *, int);
  void (* copy_from)  (void *, void *, size_t);
  void (* copy_to)    (void *, void *, size_t);
  void (* copy)       (void *, void *, size_t, hma_allocator_t *);
} fps_t;

// void * convert(
//   void * ptr, size_t size, struct hma_allocator * alloc_src,
//   struct hma_allocator * alloc_dest);

// Calculates LCM of a and b. Used to reproducibly position allocators in virtual memory
static inline int lcm(int a, int b) {
  int temp, a0 = a, b0 = b;
  while (a != 0 && b != 0) {
    temp = a % b;
    a = b;
    b = temp;
  }
  int gcd = (a > b) ? a : b;
  int lcm = a0 / gcd * b0;  // (a*b)/gcd but out of order to prevent arithmetic overflow
  return lcm;
}

// Reserves a swath of virtual for allocator and memory pool such that alignment of shared and
// device memory are both honored. Pages not readable and must be overwritten
void * reserve_memory_for_allocator(size_t shared_size, size_t dev_size, size_t dev_granularity);

// TODO: Update documentation
// Don't call this outside this library
struct hma_allocator * create_shared_allocator(
  size_t alloc_size, size_t pool_size, size_t dev_granularity, uint16_t strategy,
  uint16_t device_type, uint8_t device_number);

// TODO: Update documentation
// Do call this
struct hma_allocator * remap_shared_allocator(int shmem_id);

// Calls custom code to clean up existing memory pool, then unmaps the allocator itself
void unmap_shared_allocator(struct hma_allocator * alloc);

// copy_to, copy_from, and copy shouldn't get called on a CPU allocator, but they've been
// implemented here for completeness anyways
void cpu_copy_from(void * alloc_mem, void * cpu_mem, size_t size);
void cpu_copy_to(void * alloc_mem, void * cpu_mem, size_t size);
void cpu_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size);

extern int (*allocate_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, size_t);
extern void (*share_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, int);
extern void (*deallocate_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, int);
extern void (*copy_from_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, void *, size_t);
extern void (*copy_to_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, void *, size_t);
extern void (*copy_fps[NUM_STRATS * NUM_DEV_TYPES])(struct hma_allocator *, void *, void *, size_t);
extern struct hma_allocator * (*remap_fps[NUM_STRATS * NUM_DEV_TYPES])(struct hma_allocator *);
extern void (*unmap_fps[NUM_STRATS * NUM_DEV_TYPES])(struct hma_allocator *);

// Converts offset generated by alloc's allocate function into a pointer to type
#define GET_PTR(alloc, offset, type) \
  (type*)((uint8_t*)alloc + offset)

// Requests an allocation from alloc of size size
#define ALLOCATE(alloc, size) \
  (allocate_fps[alloc->strategy * NUM_DEV_TYPES + alloc->device_type])(alloc, size)

// Deallocates memory at offset previously generated by alloc
#define SHARE(alloc, offset) \
  (share_fps[alloc->strategy * NUM_DEV_TYPES + alloc->device_type])(alloc, offset)

// TODO: Rename to free
// Deallocates memory at offset previously generated by alloc
#define DEALLOCATE(alloc, offset) \
  (deallocate_fps[alloc->strategy * NUM_DEV_TYPES + alloc->device_type])(alloc, offset)

// Copies main memory pointer at cpu_mem into alloc_mem in alloc's domain, of size size
#define COPY_TO(alloc, alloc_mem, cpu_mem, size) \
  (copy_to_fps[alloc->strategy * NUM_DEV_TYPES + alloc->device_type])(alloc_mem, cpu_mem, size)

// Copies alloc_mem in alloc to main memory pointer at cpu_mem, of size size
#define COPY_FROM(alloc, alloc_mem, cpu_mem, size) \
  (copy_from_fps[alloc->strategy * NUM_DEV_TYPES + alloc->device_type])(alloc_mem, cpu_mem, size)

// Copies from src_alloc's pointer at src_mem into dest_alloc's pointer at dest_mem, of size size
#define COPY(dest_alloc, dest_mem, src_alloc, src_mem, size) \
  (copy_fps[src_alloc->strategy * NUM_DEV_TYPES + src_alloc->device_type])( \
    dest_alloc, dest_mem, src_mem, size)
#ifdef __cplusplus
}
#endif

#endif // HMA_ALLOCATOR_H
