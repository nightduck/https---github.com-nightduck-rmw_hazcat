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

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdatomic.h>
#include "rmw_hazcat/allocators/example_allocator.h"

struct example_allocator * create_example_allocator(size_t item_size, size_t ring_size)
{
  // NOTE: Find granularity of device your allocator is managing. This is important in order to find
  //       a reproducibly aligned range of memory. It must be a multiple of page size
  void * dev_gran = LOCAL_GRANULARITY;

  // Get 3 contiguous reservations for local, shared, and device memory. Map first two
  struct example_allocator * alloc = (struct example_allocator *)create_shared_allocator(
    NULL,
    sizeof(struct example_allocator), item_size * ring_size, dev_gran, DEVICE, ALLOC_STRAT, 0);
  // NOTE: Change DEVICE and ALLOC_STRAT above.
  // NOTE: Increase alloc_size argument if allocator has unstructured data after it, such as lookup
  //       tables and the like
  // NOTE: Modify pool_size as needed. The method will round up to the provided granularity, but
  //       you may want to increase ring_size to take advantage of that added space

  // Calculate where device mapping starts
  struct shmid_ds buf;
  if (shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    // return RMW_RET_ERROR;
    return;
  }
  void * dev_boundary = (uint8_t *)alloc + sizeof(fps_t) + buf.shm_segsz;

  // NOTE: Map device memory at end of allocator, overwriting empty reservation

  // NOTE: Construct strategy
}

int example_allocate(void * self, size_t size)
{
  struct example_allocator * s = (struct example_allocator *)self;

  // NOTE: Implement allocation method. Create internal reference counter for the allocation,
  //       (such as the 4 bytes before it), and set it to one.

  return 12345;
}

void example_share(void * self, int offset)
{
  // NOTE: Increment reference counter associated with allocation at offset.
  //       If the provided offset was not created by previous allocation, you can either return an
  //       error, or perform undefined behaviour, since that is an improper use of this method

  // If managing CPU memory, it's recommended to keep the ref counter as a header of the allocation
  // atomic_int * ref_count = (uint8_t*)self + offset - sizeof(atomic_int);
  // atomic_fetch_add(ref_count, 1);
}

void example_deallocate(void * self, int offset)
{
  struct example_allocator * s = (struct example_allocator *)self;

  // NOTE: Decrement reference counter associated with offset
  // NOTE: Implement deallocation method
}

void example_copy_from(void * here, void * there, size_t size)
{
  // NOTE: Implement method to copy from self into main memory
  //       For CPU allocators, leave unimplemented and use cpu_copy_from
}

void example_copy_to(void * here, void * there, size_t size)
{
  // NOTE: Implement method to copy to self from main memory
  //       For CPU allocators, leave unimplemented and use cpu_copy_to
}

void example_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size)
{
  // NOTE: Implement method to copy to other allocator from self
  //       For CPU allocators, leave unimplemented and use cpu_copy
}

struct hma_allocator * example_remap(struct hma_allocator * temp)
{
  // NOTE: Optional, cast the allocator to your allocator type to read from it. Do not access the
  //       fps member. All the function pointers are in unmapped memory
  struct example_allocator * alloc = temp;

  // NOTE: Find size of allocator's device memory, as well as granularity of device memory
  size_t dev_size = 0x80000;
  void * dev_gran = 0x10000;

  // Get size of shared memory
  struct shmid_ds buf;
  if (shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Remap failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Remap failed on fetching segment info");
    // return RMW_RET_ERROR;
    return;
  }

  // NOTE: You can replace these lines and reserve memory yourself, if the device requires it
  void * mapping = reserve_memory_for_allocator(buf.shm_segsz, dev_size, dev_gran);
  if (MAP_FAILED == mapping) {
    return NULL;
  }

  // Map in local portion
  void * local = mmap(
    mapping, LOCAL_GRANULARITY,
    PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (MAP_FAILED == local) {
    printf("example_remap failed on creation of local portion\n");
    handle_error("mmap");
  }

  // NOTE: If you have a CPU allocator with no device memory segment, the next 7 lines are all this
  //       function needs
  // Map in shared portion of allocator
  void * shared_mapping = shmat(temp->shmem_id, mapping + LOCAL_GRANULARITY, SHM_REMAP);
  if (MAP_FAILED == shared_mapping) {
    printf("example_remap failed on creation of shared portion\n");
    handle_error("shmat");
  }

  alloc = shared_mapping - sizeof(fps_t);
  populate_local_fn_pointers(alloc, temp->device_type << 12 | temp->strategy);

  // Calculate where device mapping starts
  void * dev_boundary = (uint8_t *)alloc + sizeof(fps_t) + buf.shm_segsz;

  // NOTE: Map in memory pool on device memory

  // alloc is partially constructed at this point. The local portion will be created and populated
  // by remap_shared_allocator, which calls this function
  return alloc;
}

void example_unmap(struct hma_allocator * alloc)
{
  // NOTE: Unmap device memory pool

  // NOTE: Any special steps needed to cleanup existing allocations

  // Unmap self, and destroy segment, if this is the last one
  struct shmid_ds buf;
  if (shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    // return RMW_RET_ERROR;
    return;
  }
  if (buf.shm_cpid == getpid()) {
    printf("Marking segment fo removal\n");
    if (shmctl(alloc->shmem_id, IPC_RMID, NULL) == -1) {
      printf("Destruction failed on marking segment for removal\n");
      // RMW_SET_ERROR_MSG("can't mark shared StaticPoolAllocator for deletion");
      // return RMW_RET_ERROR;
      return;
    }
  }
  void * shared_portion = (void *)((uint8_t *)alloc + sizeof(fps_t));
  int ret = shmdt(shared_portion);
  if (ret) {
    printf("unmap_shared_allocator, failed to detach\n");
    handle_error("shmdt");
  }

  // Remove local mapping
  if (munmap((uint8_t *)alloc + sizeof(fps_t) - LOCAL_GRANULARITY, LOCAL_GRANULARITY)) {
    printf("failed to detach local portion of allocator\n");
    handle_error("munmap");
  }
}

#ifdef __cplusplus
}
#endif
