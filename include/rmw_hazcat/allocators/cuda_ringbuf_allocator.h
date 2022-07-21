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

#ifndef RMW_HAZCAT__ALLOCATORS__CUDA_RINGBUF_ALLOCATOR_H_
#define RMW_HAZCAT__ALLOCATORS__CUDA_RINGBUF_ALLOCATOR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>

#include "rmw_hazcat/allocators/hma_template.h"

#define CUDA_RINGBUF_IMPL    CUDA << 12 | ALLOC_RING

#if defined(__linux__)
typedef int ShareableHandle;
#elif defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
typedef HANDLE ShareableHandle;
#endif

#define CUDA_RINGBUF_ALLOCATION_SIZE 0x80000000

typedef struct cuda_ringbuf_allocator
{
  union {
    struct
    {
      const fps_t fps;
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
  int pool_offset;
  ShareableHandle ipc_handle;
} cuda_ringbuf_allocator_t;

cuda_ringbuf_allocator_t * create_cuda_ringbuf_allocator(size_t item_size, size_t ring_size);

int cuda_ringbuf_allocate(void * self, size_t size);

void cuda_ringbuf_share(void * self, int offset);

void cuda_ringbuf_deallocate(void * self, int offset);

void cuda_ringbuf_copy_from(void * here, void * there, size_t size);

void cuda_ringbuf_copy_to(void * here, void * there, size_t size);

void cuda_ringbuf_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size);

struct hma_allocator * cuda_ringbuf_remap(struct hma_allocator * temp);

void cuda_ringbuf_unmap(struct hma_allocator * alloc);

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__ALLOCATORS__CUDA_RINGBUF_ALLOCATOR_H_
