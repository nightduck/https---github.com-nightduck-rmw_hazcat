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

#define _GNU_SOURCE

#include <errno.h>
#include "rmw_hazcat/allocators/hma_template.h"
#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"
#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

// Remap isn't stored in the function pointers, because it's often called on an allocator whose
// local partition isn't mapped in, so it would be useless
hma_allocator_t * (* remap_fps[NUM_STRATS * NUM_DEV_TYPES])(hma_allocator_t *) =
{
  cpu_ringbuf_remap,
  cuda_ringbuf_remap
};

void populate_local_fn_pointers(hma_allocator_t * alloc, uint32_t alloc_impl)
{
  switch (alloc_impl) {
    case CPU_RINGBUF_IMPL:
      alloc->allocate = cpu_ringbuf_allocate;
      alloc->deallocate = cpu_ringbuf_deallocate;
      alloc->share = cpu_ringbuf_share;
      alloc->copy_from = cpu_copy_from;
      alloc->copy_to = cpu_copy_to;
      alloc->copy = cpu_copy;
      alloc->unmap = cpu_ringbuf_unmap;
      break;
    case CUDA_RINGBUF_IMPL:
      alloc->allocate = cuda_ringbuf_allocate;
      alloc->deallocate = cuda_ringbuf_deallocate;
      alloc->share = cuda_ringbuf_share;
      alloc->copy_from = cuda_ringbuf_copy_from;
      alloc->copy_to = cuda_ringbuf_copy_to;
      alloc->copy = cuda_ringbuf_copy;
      alloc->unmap = cuda_ringbuf_unmap;
      break;
    default:
      // TODO(nightduck): Cleaner error handling
      assert(0);
      break;
  }
}

// Reserves a swath of virtual for allocator and memory pool such that alignment of shared and
// device memory are both honored. Pages not readable and must be overwritten
void * reserve_memory_for_allocator(size_t shared_size, size_t dev_size, size_t dev_granularity)
{
  // The allocator consists of 3 contiguous mappings: local, shared, and device memory.
  size_t local_size = LOCAL_GRANULARITY;

  // Dev pool must be aligned at an address that's multiple of this factor
  // TODO(nightduck): Added restriction that is must be power of 2?
  size_t alignment_factor = lcm(SHARED_GRANULARITY, dev_granularity);

  // A properly aligned range exists somewhere in an arbitrary mapping of this size. Reserve, but
  // don't map it
  void * rough_allocation = mmap(
    NULL, local_size + shared_size + dev_size + alignment_factor,
    PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
  if (rough_allocation == MAP_FAILED) {
    printf("reserve_memory_for_allocator couldn't reserve memroy\n");
    return NULL;
  }

  // Get aligned start for allocation within that range
  void * mapping_start = rough_allocation +
    alignment_factor - ((int64_t)rough_allocation + local_size + shared_size) % alignment_factor;

  // Trim excess
  // TODO(nightduck): Check for len = 0
  munmap(rough_allocation, mapping_start - rough_allocation);
  munmap(
    mapping_start + local_size + shared_size + dev_size,
    rough_allocation + alignment_factor - mapping_start);

  return mapping_start;
}

// TODO(nightduck): Update documentation
// Will attempt to construct local and shared partitions of allocator at address provided by hint.
// If hint is invalid, an error message is printed and null is returned. If hint is null, a default
// memory range is chosen. If hint is valid, existing mappings will be overwritten.
// Dev granularity must allways be a multiple of LOCAL_GRANULARITY, even if no dev pool is needed.
// If no dev pool is needed, just set pool_size to 0
// pool_size will be rounded up to multiple of dev_granularity
struct hma_allocator * create_shared_allocator(
  void * hint, size_t alloc_size, size_t pool_size, size_t dev_granularity, uint16_t strategy,
  uint16_t device_type, uint8_t device_number)
{
  assert(dev_granularity % LOCAL_GRANULARITY == 0);

  // The allocator consists of 3 contiguous mappings: local, shared, and device memory.
  size_t local_size = LOCAL_GRANULARITY;
  size_t shared_size = (alloc_size - sizeof(fps_t) + SHARED_GRANULARITY) -
    (alloc_size - sizeof(fps_t)) % SHARED_GRANULARITY;
  size_t dev_size = pool_size + (dev_granularity - pool_size) % dev_granularity;

  if (hint == NULL) {
    hint = reserve_memory_for_allocator(shared_size, dev_size, dev_granularity);
    if (hint == NULL) {
      return NULL;
    }
  } else {
    // Writing it this way because the ROS2 linters don't like multiline else-if statements
    if (((int64_t)hint % LOCAL_GRANULARITY) != 0 ||
      ((int64_t)hint + local_size) % SHARED_GRANULARITY != 0 ||
      ((int64_t)hint + local_size + shared_size) % dev_granularity != 0)
    {
      printf("Provided hint to create_shared_allocator isn't aligned properly\n");
      return NULL;
    }
  }

  // Make mapping for local portion of allocator (aligned at end of page)
  void * local_mapping = mmap(
    hint, LOCAL_GRANULARITY, PROT_READ | PROT_WRITE,
    MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (local_mapping == MAP_FAILED) {
    printf("failed to create local portion\n");
    handle_error("mmap");
  }

  // Create shared memory block (requested size )
  int id = shmget(IPC_PRIVATE, shared_size, 0640);
  if (id == -1) {
    // TODO(nightduck): More robust error checking
    return NULL;
  }

  // printf("Allocator id: %d\n", id);

  // Construct shared portion of allocator
  if (shmat(id, (uint8_t *)hint + LOCAL_GRANULARITY, SHM_REMAP) == MAP_FAILED) {
    printf("create_shared_allocator failed on creation of shared portion\n");
    handle_error("shmat");
  }

  // Get pointer to alloc, which won't be page aligned, but straddling different mappings
  struct hma_allocator * alloc = hint + LOCAL_GRANULARITY - sizeof(fps_t);

  // Populate with initial data
  populate_local_fn_pointers(alloc, device_type << 12 | strategy);
  alloc->shmem_id = id;
  alloc->strategy = strategy;
  alloc->device_type = device_type;
  alloc->device_number = device_number;

  // TODO(nightduck): shmctl to set permissions and such

  // Give back base allocator, straddling local and shared memory mappings, and a currently unusable
  // mapping at the end for device memory
  return alloc;
}

// TODO(nightduck): Update documentation
// Do call this
struct hma_allocator * remap_shared_allocator(int shmem_id)
{
  // Temporarily map in shared allocator to read it's alloc_type
  void * shared_portion = shmat(shmem_id, NULL, 0);
  if (shared_portion == MAP_FAILED) {
    printf("remap_shared_allocator failed on creation of shared portion\n");
    handle_error("shmat");
  }
  hma_allocator_t * temp = shared_portion - sizeof(fps_t);

  // Lookup allocator's remap function and let it bootstrap itself and any memory pool
  struct hma_allocator * alloc = REMAP(temp);

  // Unmap temp mapping, and return pointer from switch case block
  shmdt(shared_portion);

  return alloc;
}

void unmap_shared_allocator(struct hma_allocator * alloc)
{
  // Lookup allocator's unmap function and let it unmap itself and associated memory pool
  UNMAP(alloc);
}

// copy_to, copy_from, and copy shouldn't get called on a CPU allocator, but they've been
// implemented here for completeness anyways
void cpu_copy_to(void * there, void * here, size_t size)
{
  memcpy(there, here, size);
}
void cpu_copy_from(void * there, void * here, size_t size)
{
  memcpy(here, there, size);
}
void cpu_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size)
{
  COPY_TO(dest_alloc, there, here, size);
}
int cant_allocate_here(void * self, size_t size)
{
  assert(0);
  return -1;
}

#ifdef __cplusplus
}
#endif
