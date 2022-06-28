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

#define _GNU_SOURCE

#include <errno.h>
#include "rmw_hazcat/allocators/hma_template.h"
#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"
#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

int (*allocate_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, size_t) =
{
  cpu_ringbuf_allocate,
  cuda_ringbuf_allocate
};

void (*share_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, int) =
{
  cpu_ringbuf_share,
  cuda_ringbuf_share
};

void (*deallocate_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, int) =
{
  cpu_ringbuf_deallocate,
  cuda_ringbuf_deallocate
};

void (*copy_from_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, void *, size_t) =
{
  cpu_copy_from,
  cuda_ringbuf_copy_from
};

void (*copy_to_fps[NUM_STRATS * NUM_DEV_TYPES])(void *, void *, size_t) =
{
  cpu_copy_to,
  cuda_ringbuf_copy_to
};

void (*copy_fps[NUM_STRATS * NUM_DEV_TYPES])(struct hma_allocator *, void *, void *, size_t) =
{
  cpu_copy,
  cuda_ringbuf_copy
};

struct hma_allocator * (*remap_fps[NUM_STRATS * NUM_DEV_TYPES])(struct hma_allocator *) =
{
  cpu_ringbuf_remap,
  cuda_ringbuf_remap
};

void (*unmap_fps[NUM_STRATS * NUM_DEV_TYPES])(struct hma_allocator *) =
{
  cpu_ringbuf_unmap,
  cuda_ringbuf_unmap
};

// void * convert(
//   void * ptr, size_t size, struct hma_allocator * alloc_src,
//   struct hma_allocator * alloc_dest)
// {
//   if (alloc_src->domain == alloc_dest->domain) {
//     // Zero copy condition
//     return ptr;
//   } else {
//     // Allocate space on the destination allocator
//     void * here = OFFSET_TO_PTR(alloc_dest, ALLOCATE(alloc_dest, size));
//     assert(here > alloc_dest);

//     int lookup_ind = alloc_dest->strategy * NUM_DEV_TYPES + alloc_dest->device_type;

//     if (alloc_src->domain == CPU) {
//       (copy_from_fps[lookup_ind])(here, ptr, size);
//     } else if (alloc_dest->domain == CPU) {
//       (copy_to_fps[lookup_ind])(here, ptr, size);
//     } else {
//       (copy_fps[lookup_ind])(here, ptr, size, alloc_dest);
//     }
//     SHARE(alloc_src, here); // Increment reference counter
//     return here;
//   }
// }

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
      break;
    case CUDA_RINGBUF_IMPL:
      alloc->allocate = cuda_ringbuf_allocate;
      alloc->deallocate = cuda_ringbuf_deallocate;
      alloc->share = cuda_ringbuf_share;
      alloc->copy_from = cuda_ringbuf_copy_from;
      alloc->copy_to = cuda_ringbuf_copy_to;
      alloc->copy = cuda_ringbuf_copy;
      break;
    default:
      // TODO: Cleaner error handling
      assert(0);
      break;
  }
}

// TODO: Update documentation
// Hint must be sizeof(fps_t) bytes before a page boundary, and positioned such that shared portion
// of allocator and memory pool align according to their granularity requirements. If these
// conditions are not met, they will be corrected and hint will not match the returned pointer.
// If pool_size is non-zero, it'll be rounded up to a multiple of device_granularity
struct hma_allocator * create_shared_allocator(
  void * hint, size_t alloc_size, size_t dev_granularity, uint16_t strategy,
  uint16_t device_type, uint8_t device_number)
{
  // TODO: Learn more about page tables and tweak this guy. Or keep a global iterator
  if (hint == NULL) {
    hint = 0x355500000000;
  }

  // Calculate shared memory mapping
  size_t shared_portion_sz = alloc_size - sizeof(fps_t);
  shared_portion_sz += SHARED_GRANULARITY - shared_portion_sz % SHARED_GRANULARITY;

  // Round up granularity of pool (must be non-zero)
  dev_granularity += (SHARED_GRANULARITY - dev_granularity) % SHARED_GRANULARITY;

  size_t lcm_val = lcm(dev_granularity, SHARED_GRANULARITY);

  // Adjust hint so shared and device memory mappings align according to system requirements
  uint8_t * dev_pool_alignment = (uint8_t*)hint + sizeof(fps_t) + shared_portion_sz;
  dev_pool_alignment -= (int)dev_pool_alignment % lcm_val;
  hint = dev_pool_alignment - shared_portion_sz - LOCAL_GRANULARITY;

  // Make mapping for local portion of allocator (aligned at end of page)
  void * mapping_start = mmap(hint, LOCAL_GRANULARITY, PROT_READ | PROT_WRITE,
    MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

  // TODO: Learn more about page tables and tweak this guy
  while(mapping_start == MAP_FAILED && hint < 0x7f0000000000) {
    int err = errno;
    hint += 0x100000000;
    mapping_start = mmap(hint, LOCAL_GRANULARITY, PROT_READ | PROT_WRITE,
        MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  }
  if (mapping_start == MAP_FAILED) {
    printf("failed to create local portion\n");
    handle_error("mmap");
  }

  // Get pointer to alloc, which won't be page aligned, but straddling different mappings
  struct hma_allocator * alloc = mapping_start + LOCAL_GRANULARITY - sizeof(fps_t);
  populate_local_fn_pointers(alloc, device_type << 12 | strategy);

  // Create shared memory block (requested size )
  int id = shmget(IPC_PRIVATE, shared_portion_sz, 0640);
  if (id == -1) {
    // TODO: More robust error checking
    return NULL;
  }

  //printf("Allocator id: %d\n", id);

  // Construct shared portion of allocator
  if (shmat(id, (uint8_t*)mapping_start + LOCAL_GRANULARITY, 0) == MAP_FAILED) {
    printf("create_shared_allocator failed on creation of shared portion\n");
    handle_error("shmat");
  }

  // Previous pointer should now work in shared partition
  alloc->shmem_id = id;
  alloc->strategy = strategy;
  alloc->device_type = device_type;
  alloc->device_number = device_number;

  //printf("Mounted shared portion of alloc at: %xp\n", alloc);

  // TODO: shmctl to set permissions and such

  // Give back base allocator, straddling local and shared memory mappings
  return alloc;
}

// TODO: Update documentation
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

  // TODO: Modify specs for individual remap to use a PROT_NONE mapping spanning local, shared, and
  //       device mappings, then munmap unaligned portions, and then overwrite desired ranges with
  //       MAP_FIXED and SHM_REMAP (latter isn't posix portable)
  // Lookup allocator's remap function and let it bootstrap itself and any memory pool
  int lookup_ind = temp->strategy * NUM_DEV_TYPES + temp->device_type;
  struct hma_allocator * alloc = (remap_fps[lookup_ind])(temp);

  // Map in local portion
  void * local = mmap((uint8_t*)alloc + sizeof(fps_t) - LOCAL_GRANULARITY, LOCAL_GRANULARITY,
    PROT_READ | PROT_WRITE, MAP_FIXED_NOREPLACE | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (local == MAP_FAILED) {
    printf("remap_shared_allocator failed on creation of local portion\n");
    handle_error("mmap");
  }
  populate_local_fn_pointers(alloc, temp->device_type << 12 | temp->strategy);

  // Unmap temp mapping, and return pointer from switch case block
  shmdt(shared_portion);

  return alloc;
}

void unmap_shared_allocator(struct hma_allocator * alloc)
{
  // Lookup allocator's unmap function and let it unmap itself and associated memory pool
  int lookup_ind = alloc->strategy * NUM_DEV_TYPES + alloc->device_type;
  (unmap_fps[lookup_ind])(alloc);   // TODO: Add unmap calls to the fps_t struct

  if(munmap((uint8_t*)alloc + sizeof(fps_t) - LOCAL_GRANULARITY, LOCAL_GRANULARITY)) {
    printf("failed to detach local portion of allocator\n");
    handle_error("munmap");
  }
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
