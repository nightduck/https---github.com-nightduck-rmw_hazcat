#ifdef __cplusplus
extern "C"
{
#endif

#include "rmw_hazcat/allocators/example_allocator.h"

struct example_allocator * create_example_allocator(size_t item_size, size_t ring_size)
{
  // TODO: Find address you want your allocator to start at. Might be determined by the memory
  //       granularity of the device it is managing. Shouldn't overlap with temp
  void * hint = NULL;

  // TODO: Maybe reserve an address space if your device API requires it

  struct example_allocator * alloc = (struct example_allocator *)create_shared_allocator(
    hint, sizeof(struct example_allocator), DEVICE, ALLOC_STRAT, 0);
  // TODO: Change DEVICE and ALLOC_STRAT above

  // TODO: Construct strategy
}

int example_allocate(void * self, size_t size)
{
  struct example_allocator * s = (struct example_allocator *)self;

  // TODO: Implement allocation method

  return 12345;
}

void example_deallocate(void * self, int offset)
{
  struct example_allocator * s = (struct example_allocator *)self;

  // TODO: Implement deallocation method
}

void example_copy_from(void * there, void * here, size_t size)
{
  // TODO: Implement method to copy from main memory into self
}

void example_copy_to(void * there, void * here, size_t size)
{
  // TODO: Implement method to copy to main memory from self
}

void example_copy(void * there, void * here, size_t size, struct hma_allocator * dest_alloc)
{
  // TODO: Implement method to copy to non-cpu memory from self
}

struct hma_allocator * example_remap(struct hma_allocator * temp)
{
  // TODO: Optional, cast the allocator to your allocator type to read from it
  struct example_allocator * alloc = temp;

  // TODO: Find address you want your allocator to start at. Might be determined by the memory
  //       granularity of the device it is managing. Shouldn't overlap with temp
  void * hint = NULL;

  // TODO: Maybe reserve an address space if your device API requires it

  // Map in shared portion of allocator
  alloc = shmat(temp->shmem_id, hint, 0);

  // TODO: Map in memory pool on device memory

  // fps can now be typecast to example_allocator* and work correctly. Updates to any member
  // besides top 40 bytes will be visible across processes
  return alloc;
}

#ifdef __cplusplus
}
#endif
