#ifndef CPU_RINGBUF_ALLOCATOR_H
#define CPU_RINGBUF_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define CPU_RINGBUF_IMPL    CPU << 12 | ALLOC_RING

#include "hma_template.h"


struct cpu_ringbuf_allocator {
  union {
    struct {
      // Exist in local memory, pointing to static functions
      int   (*const allocate)   (void * self, size_t size);
      void  (*const deallocate) (void * self, int offset);
      void  (*const copy_from)  (void * here, void * there, size_t size);
      void  (*const copy_to)    (void * here, void * there, size_t size);
      void  (*const copy)       (void * here, void * there, size_t size, struct hma_allocator * dest_alloc);

      // Exist in shared memory
      const int shmem_id;
      const uint32_t alloc_type;
    };
    struct hma_allocator untyped;
  };

  int count;
  int rear_it;
  int item_size;
  int ring_size;
};

struct cpu_ringbuf_allocator * create_cpu_ringbuf_allocator(size_t item_size, size_t ring_size);

int cpu_ringbuf_allocate(void* self, size_t size);

void cpu_ringbuf_deallocate(void* self, int offset);

struct hma_allocator * cpu_ringbuf_remap(struct shared * temp);

#ifdef __cplusplus
}
#endif

#endif // CPU_RINGBUF_ALLOCATOR_H