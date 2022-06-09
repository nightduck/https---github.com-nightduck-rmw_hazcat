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

void example_copy_from(void * here, void * there, size_t size)
{
  // TODO: Implement method to copy from self into main memory
  //       For CPU allocators, leave unimplemented and use cpu_copy_from
}

void example_copy_to(void * here, void * there, size_t size)
{
  // TODO: Implement method to copy to self from main memory
  //       For CPU allocators, leave unimplemented and use cpu_copy_to
}

void example_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size)
{
  // TODO: Implement method to copy to other allocator from self
  //       For CPU allocators, leave unimplemented and use cpu_copy
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

void example_unmap(struct hma_allocator * alloc)
{
  // TODO: Any special steps needed to cleanup existing allocations

  // Unmap self, and destroy segment, if this is the last one
  struct shmid_ds buf;
  if(shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    //RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    //return RMW_RET_ERROR;
    return;
  }
  if(buf.shm_cpid == getpid()) {
    printf("Marking segment fo removal\n");
    if(shmctl(alloc->shmem_id, IPC_RMID, NULL) == -1) {
        printf("Destruction failed on marking segment for removal\n");
        //RMW_SET_ERROR_MSG("can't mark shared StaticPoolAllocator for deletion");
        //return RMW_RET_ERROR;
        return;
    }
  }
  int ret = shmdt(alloc);
  if(ret) {
    printf("cpu_ringbuf_unmap, failed to detach\n");
    handle_error("shmdt");
  }
}

#ifdef __cplusplus
}
#endif
