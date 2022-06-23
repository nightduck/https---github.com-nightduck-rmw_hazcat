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

// TODO: Update documentation
// Don't call this outside this library
struct hma_allocator * create_shared_allocator(
  void * hint, size_t alloc_size, uint16_t strategy,
  uint16_t device_type, uint8_t device_number)
{
  // Create shared memory block (requested size )
  int id = shmget(IPC_PRIVATE, alloc_size, 0640);
  if (id == -1) {
    // TODO: More robust error checking
    return NULL;
  }

  printf("Allocator id: %d\n", id);

  // Construct shared portion of allocator
  int remap = (hint == NULL) ? 0 : SHM_REMAP;
  struct hma_allocator * alloc = (struct hma_allocator *)shmat(id, hint, remap);
  if (alloc == MAP_FAILED) {
    printf("create_shared_allocator failed on creation of shared portion\n");
    handle_error("shmat");
  }
  alloc->shmem_id = id;
  alloc->strategy = strategy;
  alloc->device_type = device_type;
  alloc->device_number = device_number;

  printf("Mounted shared portion of alloc at: %xp\n", alloc);

  // TODO: shmctl to set permissions and such

  // Give back base allocator, straddling local and shared memory mappings
  return alloc;
}

// TODO: Update documentation
// Do call this
struct hma_allocator * remap_shared_allocator(int shmem_id)
{
  // Temporarily map in shared allocator to read it's alloc_type
  struct hma_allocator * temp = (struct hma_allocator *)shmat(shmem_id, NULL, 0);

  // Lookup allocator's remap function and let it bootstrap itself and any memory pool
  int lookup_ind = temp->strategy * NUM_DEV_TYPES + temp->device_type;
  struct hma_allocator * alloc = (remap_fps[lookup_ind])(temp);

  // Unmap temp mapping, and return pointer from switch case block
  shmdt(temp);

  return alloc;
}

void unmap_shared_allocator(struct hma_allocator * alloc)
{
  // Lookup allocator's unmap function and let it unmap itself and associated memory pool
  int lookup_ind = alloc->strategy * NUM_DEV_TYPES + alloc->device_type;
  (unmap_fps[lookup_ind])(alloc);
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
