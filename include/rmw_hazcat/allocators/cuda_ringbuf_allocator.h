#ifndef CUDA_RINGBUF_ALLOCATOR_H
#define CUDA_RINGBUF_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define CUDA_RINGBUF_IMPL    CUDA << 12 | ALLOC_RING

#include "hma_template.h"
#include <stdio.h>

#if defined(__linux__)
typedef int ShareableHandle;
#elif defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
typedef HANDLE ShareableHandle;
#endif

#define CUDA_RINGBUF_ALLOCATION_SIZE 0x80000000

struct cuda_ringbuf_allocator
{
  union {
    struct
    {
      const int shmem_id;
      const uint16_t device_type;
      const uint16_t device_number;
      const uint16_t strategy;
    };
    struct hma_allocator untyped;
  };

  int count;
  int rear_it;
  int item_size;
  int ring_size;
  ShareableHandle ipc_handle;
};

struct cuda_ringbuf_allocator * create_cuda_ringbuf_allocator(size_t item_size, size_t ring_size);

int cuda_ringbuf_allocate(void * self, size_t size);

void cuda_ringbuf_deallocate(void * self, int offset);

void cuda_ringbuf_copy_from(void * here, void * there, size_t size);

void cuda_ringbuf_copy_to(void * here, void * there, size_t size);

void cuda_ringbuf_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size);

struct hma_allocator * cuda_ringbuf_remap(struct hma_allocator * temp);

void cuda_ringbuf_unmap(struct hma_allocator * alloc);

#ifdef __cplusplus
}
#endif

#endif // CUDA_RINGBUF_ALLOCATOR_H
