#ifndef EXAMPLE_ALLOCATOR_H
#define EXAMPLE_ALLOCATOR_H

#ifdef __cplusplus
extern "C"
{
#endif

#define EXAMPLE_IMPL    DEVICE << 12 | ALLOC_STRAT

#include "hma_template.h"

// TODO: Ctrl+H to replace all "EXAMPLE" and "example" with the name of your allocator

struct example_allocator
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

  // TODO: Populate this portion with any structures to implement your strategy
};

// TODO: Implement the functions below in source file example_allocator.c

struct example_allocator * create_example_allocator(size_t item_size, size_t ring_size);

int example_allocate(void * self, size_t size);

void example_deallocate(void * self, int offset);

void example_copy_from(void * there, void * here, size_t size);

void example_copy_to(void * there, void * here, size_t size);

void example_copy(void * there, void * here, size_t size, struct hma_allocator * dest_alloc);

struct hma_allocator * example_remap(struct hma_allocator * temp);

void example_unmap(struct hma_allocator * alloc);

#ifdef __cplusplus
}
#endif

#endif // EXAMPLE_ALLOCATOR_H
