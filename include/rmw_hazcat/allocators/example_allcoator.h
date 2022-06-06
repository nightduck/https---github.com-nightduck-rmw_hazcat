#ifndef EXAMPLE_ALLOCATOR_H
#define EXAMPLE_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define EXAMPLE_IMPL    DEVICE << 12 | ALLOC_STRAT

#include "hma_template.h"

// TODO: Ctrl+H to replace all "EXAMPLE" and "example" with the name of your allocator

struct example_allocator {
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

  // TODO: Populate this portion with any structures to implement your strategy
};

// TODO: Implement the functions below in source file example_allocator.c

struct example_allocator * create_example_allocator(size_t item_size, size_t ring_size);

int example_allocate(void* self, size_t size);

void example_deallocate(void* self, int offset);

void example_copy_from(void* there, void* here, size_t size);

void example_copy_to(void* there, void* here, size_t size);

void example_copy(void* there, void* here, size_t size, struct hma_allocator * dest_alloc);

struct hma_allocator * example_remap(struct shared * temp);

#ifdef __cplusplus
}
#endif

#endif // EXAMPLE_ALLOCATOR_H