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

#include <cuda_runtime_api.h>
#include <cuda.h>

#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

static inline void
checkDrvError(CUresult res, const char * tok, const char * file, unsigned line)
{
  if (CUDA_SUCCESS != res) {
    const char * errStr = NULL;
    (void)cuGetErrorString(res, &errStr);
    printf("%s:%d %sfailed (%d): %s\n", file, line, tok, (unsigned)res, errStr);
    abort();
  }
}

#define CHECK_DRV(x) checkDrvError(x, #x, __FILE__, __LINE__);

cuda_ringbuf_allocator_t * create_cuda_ringbuf_allocator(size_t item_size, size_t ring_size)
{
  CUmemAllocationProp props = {};
  props.type = CU_MEM_ALLOCATION_TYPE_PINNED;
  props.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  props.location.id = 0;
  props.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
  size_t gran;
  CHECK_DRV(cuMemGetAllocationGranularity(&gran, &props, CU_MEM_ALLOC_GRANULARITY_MINIMUM));

  int devCount;
  CHECK_DRV(cuDeviceGetCount(&devCount));

  CUdevice device;
  CHECK_DRV(cuDeviceGet(&device, 0));

  size_t rough_size = item_size * ring_size;
  size_t remainder = rough_size % gran;
  size_t pool_size = (remainder == 0) ? rough_size : rough_size + gran - remainder;

  size_t alloc_size = sizeof(struct cuda_ringbuf_allocator) + sizeof(atomic_int) * ring_size;
  size_t shared_size = (alloc_size - sizeof(fps_t) + SHARED_GRANULARITY) -
    (alloc_size - sizeof(fps_t)) % SHARED_GRANULARITY;
  size_t alignment_factor = lcm(SHARED_GRANULARITY, gran);

  // Reserve virtual memory range large enough to accommodate 3 appropriate aligned partitions
  size_t reservation_size = LOCAL_GRANULARITY + shared_size + pool_size + alignment_factor;
  reservation_size += gran - (reservation_size % gran);   // Round up
  CUdeviceptr d_addr;
  CHECK_DRV(
    cuMemAddressReserve(
      &d_addr,
      reservation_size,
      alignment_factor,
      NULL, 0ULL));

  // Find actual address we're going to construct the allocator at (must be aligned right)
  void * hint = (void *)(uintptr_t)d_addr + alignment_factor -
    ((int64_t)(uintptr_t)d_addr + LOCAL_GRANULARITY + shared_size) % alignment_factor;

  struct cuda_ringbuf_allocator * alloc = (struct cuda_ringbuf_allocator *)create_shared_allocator(
    hint, alloc_size, pool_size, gran, ALLOC_RING, CUDA, 0);

  // Since CUDA insisted on reserving memory for us that probably isn't aligned right, keep a note
  // of it for later
  alloc->untyped.data = (void *)(uintptr_t)d_addr;

  struct shmid_ds buf;
  if (shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    // return RMW_RET_ERROR;
    return;
  }
  CUdeviceptr dev_boundary = (CUdeviceptr)(uintptr_t)
    ((uint8_t *)alloc + sizeof(fps_t) + buf.shm_segsz);

  CUmemGenericAllocationHandle original_handle;
  CHECK_DRV(cuMemCreate(&original_handle, pool_size, &props, device));

  // Export to create shared handle.
  ShareableHandle ipc_handle;
  CHECK_DRV(
    cuMemExportToShareableHandle(
      (void *)&ipc_handle, original_handle,
      CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR, 0));

  // cuMemMap
  CHECK_DRV(cuMemMap(dev_boundary, pool_size, 0, original_handle, 0));

  // cuMemSetAccess
  CUmemAccessDesc accessDesc;
  accessDesc.location.id = 0;
  accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
  CHECK_DRV(cuMemSetAccess(dev_boundary, pool_size, &accessDesc, 1));

  // Free handle. Memory will stay valid as long as it is mapped
  CHECK_DRV(cuMemRelease(original_handle));

  // Construct strategy
  alloc->count = 0;
  alloc->rear_it = 0;
  alloc->item_size = item_size;
  alloc->ring_size = ring_size;   // TODO(nightduck): Expand to fill rest of page
  alloc->ipc_handle = ipc_handle;
  alloc->pool_offset = (uint8_t *)(uintptr_t)dev_boundary - (uint8_t *)alloc;
  return alloc;
}

int cuda_ringbuf_allocate(void * self, size_t size)
{
  struct cuda_ringbuf_allocator * s = (struct cuda_ringbuf_allocator *)self;

  if (s->count == s->ring_size) {
    // Allocator full
    return -1;
  } else {
    int forward_it = (s->rear_it + s->count) % s->ring_size;

    // Set the reference counter to be 1
    atomic_uint * ref_count = (uint8_t *)self + sizeof(struct cuda_ringbuf_allocator);
    ref_count[forward_it] = 1;

    // TODO(nightduck): Redo this to consider page alignment and the array of reference pointers in
    // CPU memory. Give address relative to shared object
    int ret = s->pool_offset + s->item_size * forward_it;

    // Update count of how many elements in pool
    s->count++;

    return ret;
  }

  return 12345;
}

void cuda_ringbuf_share(void * self, int offset)
{
  struct cuda_ringbuf_allocator * s = (struct cuda_ringbuf_allocator *)self;

  // TODO(nightduck): Redo this to consider page alignment
  int index = (offset - s->pool_offset) / s->item_size;
  atomic_uint * ref_count = (uint8_t *)self + sizeof(struct cuda_ringbuf_allocator);
  atomic_fetch_add(&ref_count[index], 1);
}

void cuda_ringbuf_deallocate(void * self, int offset)
{
  struct cuda_ringbuf_allocator * s = (struct cuda_ringbuf_allocator *)self;
  if (s->count == 0) {
    return;       // Allocator empty, nothing to deallocate
  }

  // TODO(nightduck): Redo this to consider page alignment
  int entry = (offset - s->pool_offset) / s->item_size;

  // Decrement reference counter and only go through with deallocate if it's zero
  atomic_uint * ref_count = (uint8_t *)self + sizeof(struct cuda_ringbuf_allocator);
  if (--ref_count[entry] > 0) {
    return;
  }

  // Do math with imaginary overflow indices so forward_it >= entry >= rear_it
  int forward_it = s->rear_it + s->count;
  if (__glibc_unlikely(entry < s->rear_it)) {
    entry += s->ring_size;
  }

  // Most likely scenario: entry == rear_it as allocations are deallocated in order
  s->rear_it = entry + 1;
  s->count = forward_it - s->rear_it;
  s->rear_it %= s->ring_size;
}

void cuda_ringbuf_copy_from(void * here, void * there, size_t size)
{
  cudaMemcpy(there, here, size, cudaMemcpyDeviceToHost);
}

void cuda_ringbuf_copy_to(void * here, void * there, size_t size)
{
  cudaMemcpy(here, there, size, cudaMemcpyHostToDevice);
}

void cuda_ringbuf_copy(struct hma_allocator * dest_alloc, void * there, void * here, size_t size)
{
  void * interm = malloc(size);
  cuda_ringbuf_copy_from(here, interm, size);
  COPY_TO(dest_alloc, there, interm, size);
}

struct hma_allocator * cuda_ringbuf_remap(struct hma_allocator * temp)
{
  struct cuda_ringbuf_allocator * cuda_alloc = (struct cuda_ringbuf_allocator *)temp;

  // TODO(nightduck): Add requirement to allocator spec to define function to get device gran
  CUmemAllocationProp props = {};
  props.type = CU_MEM_ALLOCATION_TYPE_PINNED;
  props.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  props.location.id = 0;
  props.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
  size_t gran;
  CHECK_DRV(cuMemGetAllocationGranularity(&gran, &props, CU_MEM_ALLOC_GRANULARITY_MINIMUM));

  size_t rough_size = cuda_alloc->item_size * cuda_alloc->ring_size;
  size_t remainder = rough_size % gran;
  size_t pool_size = (remainder == 0) ? rough_size : rough_size + gran - remainder;
  size_t alignment_factor = lcm(SHARED_GRANULARITY, gran);

  struct shmid_ds buf;
  if (shmctl(cuda_alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Remap failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Remap failed on fetching segment info");
    // return RMW_RET_ERROR;
    return NULL;
  }

  // Reserve virtual memory range large enough to accommodate 3 appropriate aligned partitions
  size_t reservation_size = LOCAL_GRANULARITY + buf.shm_segsz + pool_size + alignment_factor;
  reservation_size += gran - (reservation_size % gran);   // Round up
  CUdeviceptr d_addr;
  CHECK_DRV(
    cuMemAddressReserve(
      &d_addr,
      reservation_size,
      0,
      NULL, 0ULL));

  // Find actual address we're going to construct the allocator at (must be aligned right)
  void * mapping = (void *)(uintptr_t)d_addr + alignment_factor -
    ((int64_t)(uintptr_t)d_addr + LOCAL_GRANULARITY + buf.shm_segsz) % alignment_factor;

  // Map in local portion
  void * local = mmap(
    mapping, LOCAL_GRANULARITY,
    PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (MAP_FAILED == local) {
    printf("cuda_ringbuf_remap failed on creation of local portion\n");
    handle_error("mmap");
  }

  // Map in shared portion of allocator
  void * shared_mapping = shmat(temp->shmem_id, mapping + LOCAL_GRANULARITY, SHM_REMAP);
  if (MAP_FAILED == shared_mapping) {
    printf("cuda_ringbuf_remap failed on creation of shared portion\n");
    handle_error("shmat");
  }

  // Get pointer to remapped allocator, and populate local mapping
  cuda_alloc = shared_mapping - sizeof(fps_t);
  populate_local_fn_pointers(cuda_alloc, temp->device_type << 12 | temp->strategy);
  cuda_alloc->untyped.data = (void *)(uintptr_t)d_addr;

  // Get shareable handle
  CUmemGenericAllocationHandle handle;
  CHECK_DRV(
    cuMemImportFromShareableHandle(
      &handle, (void *)(uintptr_t)(cuda_alloc->ipc_handle),
      CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR));

  // Calculate where device mapping starts
  void * dev_boundary = (uint8_t *)shared_mapping + buf.shm_segsz;

  // cuMemMap
  CHECK_DRV(cuMemMap((CUdeviceptr)(uintptr_t)dev_boundary, pool_size, 0, handle, 0));

  // cuMemSetAccess
  CUmemAccessDesc accessDesc;
  accessDesc.location.id = 0;
  accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
  CHECK_DRV(cuMemSetAccess(dev_boundary, pool_size, &accessDesc, 1));

  // Free handle. Memory will stay valid as long as it is mapped
  CHECK_DRV(cuMemRelease(handle));

  // alloc is partially constructed at this point. The local portion will be created and populated
  // by remap_shared_allocator, which calls this function
  return cuda_alloc;
}

void cuda_ringbuf_unmap(struct hma_allocator * alloc)
{
  cuda_ringbuf_allocator_t * cuda_alloc = (cuda_ringbuf_allocator_t *)alloc;
  CUdeviceptr reservation = (CUdeviceptr)(uintptr_t)alloc->data;

  // Get info on shared segment
  struct shmid_ds buf;
  if (shmctl(alloc->shmem_id, IPC_STAT, &buf) == -1) {
    printf("Destruction failed on fetching segment info\n");
    // RMW_SET_ERROR_MSG("Error reading info about shared StaticPoolAllocator");
    // return RMW_RET_ERROR;
    return;
  }

  // Calculate some numbers that'll help find out how big the CUDA reservation was
  CUmemAllocationProp props = {};
  props.type = CU_MEM_ALLOCATION_TYPE_PINNED;
  props.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  props.location.id = 0;
  props.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
  size_t gran;
  CHECK_DRV(cuMemGetAllocationGranularity(&gran, &props, CU_MEM_ALLOC_GRANULARITY_MINIMUM));
  size_t rough_size = cuda_alloc->item_size * cuda_alloc->ring_size;
  size_t remainder = rough_size % gran;
  size_t pool_size = (remainder == 0) ? rough_size : rough_size + gran - remainder;
  size_t alignment_factor = lcm(SHARED_GRANULARITY, gran);

  // Unmap device memory
  CUdeviceptr dev_boundary = (CUdeviceptr)(uintptr_t)
    ((uint8_t *)alloc + sizeof(fps_t) + buf.shm_segsz);
  CHECK_DRV(cuMemUnmap(dev_boundary, pool_size));

  // Unmap self, and destroy segment, if this is the last one
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

  // Unmap local portion
  if (munmap((uint8_t *)alloc + sizeof(fps_t) - LOCAL_GRANULARITY, LOCAL_GRANULARITY)) {
    printf("failed to detach local portion of allocator\n");
    handle_error("munmap");
  }

  size_t reservation_size = LOCAL_GRANULARITY + buf.shm_segsz + pool_size + alignment_factor;
  reservation_size += gran - (reservation_size % gran);   // Round up
  CHECK_DRV(cuMemAddressFree(reservation, reservation_size));
}

#ifdef __cplusplus
}
#endif
