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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdatomic.h>
#include "rmw_hazcat/allocators/example_allocator.h"

struct example_allocator * create_example_allocator(size_t item_size, size_t ring_size)
{
  // TODO: Find address you want your allocator to start at. Might be determined by the memory
  //       granularity of the device it is managing. Shouldn't overlap with temp
  void * hint = NULL;

  struct example_allocator * alloc = (struct example_allocator *)create_shared_allocator(
    hint, sizeof(struct example_allocator), DEVICE, ALLOC_STRAT, 0);
  // TODO: Change DEVICE and ALLOC_STRAT above

  // TODO: Reserve device memory at end of allocator

  // TODO: Construct strategy
}

int example_allocate(void * self, size_t size)
{
  struct example_allocator * s = (struct example_allocator *)self;

  // TODO: Implement allocation method. Create internal reference counter for the allocation,
  //       (such as the 4 bytes before it), and set it to one.

  return 12345;
}

void example_share(void * self, int offset) {
  // TODO: Increment reference counter associated with allocation at offset.
  //       If the provided offset was not created by previous allocation, you can either return an
  //       error, or perform undefined behaviour, since that is an improper use of this method

  // If managing CPU memory, it's recommended to keep the ref counter as a header of the allocation
  // atomic_int * ref_count = (uint8_t*)self + offset - sizeof(atomic_int);
  // atomic_fetch_add(ref_count, 1);
}

void example_deallocate(void * self, int offset)
{
  struct example_allocator * s = (struct example_allocator *)self;

  // TODO: Decrement reference counter associated with offset
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
  // TODO: Optional, cast the allocator to your allocator type to read from it. Do not access the
  //       fps member. All the function pointers are in unmapped memory
  struct example_allocator * alloc = temp;

  // TODO: Find address you want your allocator to start at. Might be determined by the memory
  //       granularity of the device it is managing. Shouldn't overlap with temp
  void * hint = NULL;

  // TODO: Maybe reserve an address space if your device API requires it

  // Map in shared portion of allocator
  alloc = shmat(temp->shmem_id, hint, 0) - sizeof(fps_t);
  if (alloc == MAP_FAILED) {
    printf("example_remap failed on creation of shared portion\n");
    handle_error("shmat");
  }

  // TODO: Map in memory pool on device memory

  // alloc is partially constructed at this point. The local portion will be created and populated
  // by remap_shared_allocator, which calls this function
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
  void * shared_portion = (void*)((uint8_t*)alloc + sizeof(fps_t));
  int ret = shmdt(shared_portion);
  if(ret) {
    printf("cpu_ringbuf_unmap, failed to detach\n");
    handle_error("shmdt");
  }
}

#ifdef __cplusplus
}
#endif
