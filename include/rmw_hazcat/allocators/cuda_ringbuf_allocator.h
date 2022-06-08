#ifndef CUDA_RINGBUF_ALLOCATOR_H
#define CUDA_RINGBUF_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define CUDA_RINGBUF_IMPL    CUDA << 12 | ALLOC_RING

#include "hma_template.h"
#include <cuda_runtime_api.h>
#include <cuda.h>
#include <stdio.h>


static inline void
checkDrvError(CUresult res, const char * tok, const char * file, unsigned line)
{
  if (res != CUDA_SUCCESS) {
    const char * errStr = NULL;
    (void)cuGetErrorString(res, &errStr);
    printf("%s:%d %sfailed (%d): %s\n", file, line, tok, (unsigned)res, errStr);
    abort();
  }
}

#define CHECK_DRV(x) checkDrvError(x, #x, __FILE__, __LINE__);

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
      const uint16_t strategy : 12;
      const uint16_t device_type : 12;
      const uint8_t device_number;
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

void cuda_ringbuf_copy_from(void * there, void * here, size_t size);

void cuda_ringbuf_copy_to(void * there, void * here, size_t size);

void cuda_ringbuf_copy(void * there, void * here, size_t size, struct hma_allocator * dest_alloc);

struct hma_allocator * cuda_ringbuf_remap(struct hma_allocator * temp);

void cuda_ringbuf_unmap(struct hma_allocator * alloc);

#ifdef __cplusplus
}
#endif

#endif // CUDA_RINGBUF_ALLOCATOR_H
