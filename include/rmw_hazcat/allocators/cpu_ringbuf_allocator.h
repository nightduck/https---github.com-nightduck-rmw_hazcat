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
    struct {
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
};

struct cpu_ringbuf_allocator * create_cpu_ringbuf_allocator(size_t item_size, size_t ring_size);

int cpu_ringbuf_allocate(void * self, size_t size);

void cpu_ringbuf_deallocate(void * self, int offset);

struct hma_allocator * cpu_ringbuf_remap(struct hma_allocator * temp);

#ifdef __cplusplus
}
#endif

#endif // CPU_RINGBUF_ALLOCATOR_H
