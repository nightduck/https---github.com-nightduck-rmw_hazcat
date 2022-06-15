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

#ifndef CPU_RINGBUF_ALLOCATOR_H
#define CPU_RINGBUF_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define CPU_RINGBUF_IMPL    CPU << 12 | ALLOC_RING

#include "hma_template.h"


struct cpu_ringbuf_allocator
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
};

struct cpu_ringbuf_allocator * create_cpu_ringbuf_allocator(size_t item_size, size_t ring_size);

int cpu_ringbuf_allocate(void * self, size_t size);

void cpu_ringbuf_deallocate(void * self, int offset);

struct hma_allocator * cpu_ringbuf_remap(struct hma_allocator * temp);

void cpu_ringbuf_unmap(struct hma_allocator * alloc);

#ifdef __cplusplus
}
#endif

#endif // CPU_RINGBUF_ALLOCATOR_H
