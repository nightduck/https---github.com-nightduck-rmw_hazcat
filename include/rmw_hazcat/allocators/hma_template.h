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
#include <string.h>
#include <sys/shm.h>
#include <sys/mman.h>


#define OFFSET_TO_PTR(a, o) (uint8_t *)a + o
#define PTR_TO_OFFSET(a, p) (uint8_t *)p - (uint8_t *)a

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define MAX_POOL_SIZE   0x100000000

// alloc_type is 32bit value.
// First 12 bits encode allocator strategy
// Next 12 bits encode device type
// Last 8 bits encode device number. Only used for multiple GPUs, etc. Must be zero for CPU
//
// First 24 bits collectively encode the needed allocator implementation.
// The last 20 bits collectively encode the memory "domain"
//
// Example definition:  alloc_type x = (alloc_type)(ALLOC_TLSF << 20 | CUDA << 8 | 3);

union alloc_type {
  struct
  {
    uint16_t strategy : 12;
    union {
      uint16_t device_type : 12;
      uint32_t domain : 20;
    };
  };
  struct
  {
    uint32_t alloc_impl : 24;
    uint8_t device_number;
  };
  uint32_t raw;
};

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


/*
  // Copy paste at head of new allocators, so first 8 bytes can be cast as a hma_allocator
  union {
    struct {
      const int shmem_id;
      const uint16_t strategy : 12;
      const uint16_t device_type : 12;
      const uint8_t device_number;
    };
    struct hma_allocator untyped;
  };
*/

struct hma_allocator
{
  int shmem_id;
  union              // 32bit int indicating type of allocator and memory domain
  {
    struct
    {
      uint16_t strategy : 12;
      union {
        uint16_t device_type : 12;
        uint32_t domain : 20;
      };
    };
    struct
    {
      uint32_t alloc_impl : 24;
      uint8_t device_number;
    };
  };
};

// Wrapper to lookup and call appropriate allocate function for provided allocator
int allocate(struct hma_allocator * alloc, size_t size);

// Wrapper to lookup and call appropriate deallocate function for provided allocator
void deallocate(struct hma_allocator * alloc, int offset);

void * convert(
  void * ptr, size_t size, struct hma_allocator * alloc_src,
  struct hma_allocator * alloc_dest);

// copy_to, copy_from, and copy shouldn't get called on a CPU allocator, but they've been
// implemented here for completeness anyways
void cpu_copy_tofrom(void * there, void * here, size_t size);
void cpu_copy(void * there, void * here, size_t size, struct hma_allocator * dest_alloc);
int cant_allocate_here(void * self, size_t size);

// TODO: Update documentation
// Don't call this outside this library
struct hma_allocator * create_shared_allocator(
  void * hint, size_t alloc_size, uint16_t strategy,
  uint16_t device_type, uint8_t device_number);

// TODO: Update documentation
// Do call this
struct hma_allocator * remap_shared_allocator(int shmem_id);

#ifdef __cplusplus
}
#endif

#endif // HMA_ALLOCATOR_H
