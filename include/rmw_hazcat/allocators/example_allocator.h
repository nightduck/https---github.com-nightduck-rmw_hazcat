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

#ifndef RMW_HAZCAT__ALLOCATORS__EXAMPLE_ALLOCATOR_H_
#define RMW_HAZCAT__ALLOCATORS__EXAMPLE_ALLOCATOR_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include "rmw_hazcat/allocators/hma_template.h"

#define EXAMPLE_IMPL    DEVICE << 12 | ALLOC_STRAT

// NOTE: Specify allocation granularity for managed device. Memory pool will be aligned
//       with multiple of this values
#define DEVICE_MEMPOOL_GRANULARITY     _SC_PAGE_SIZE

// NOTE: Ctrl+H to replace all "EXAMPLE" and "example" with the name of your allocator

struct example_allocator
{
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

  // NOTE: Populate this portion with any structures to implement your strategy
};

// NOTE: Implement the functions below in source file example_allocator.c

struct example_allocator * create_example_allocator(size_t item_size, size_t ring_size);

int example_allocate(void * self, size_t size);

void example_share(void * self, int offset);

void example_deallocate(void * self, int offset);

void example_copy_from(void * here, void * there, size_t size);

void example_copy_to(void * here, void * there, size_t size);

void example_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size);

struct hma_allocator * example_remap(struct hma_allocator * temp);

void example_unmap(struct hma_allocator * alloc);

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__ALLOCATORS__EXAMPLE_ALLOCATOR_H_
