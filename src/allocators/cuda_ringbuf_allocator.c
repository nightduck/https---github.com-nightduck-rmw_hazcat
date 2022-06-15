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

#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"
#include <cuda_runtime_api.h>
#include <cuda.h>

static inline void
checkDrvError(CUresult res, const char * tok, const char * file, unsigned line)
{
  if (res != CUDA_SUCCESS) {
    const char * errStr = NULL;
    (void)cuGetErrorString(res, &errStr);
    printf("%s:%d %sfailed (%d): %s\n", file, line, tok, (unsigned)res, errStr);
    abort();
  }
}

#define CHECK_DRV(x) checkDrvError(x, #x, __FILE__, __LINE__);

struct cuda_ringbuf_allocator * create_cuda_ringbuf_allocator(size_t item_size, size_t ring_size)
{
  void * hint = NULL;

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

  CUmemGenericAllocationHandle original_handle;
  CHECK_DRV(cuMemCreate(&original_handle, pool_size, &props, device));

  // Export to create shared handle.
  ShareableHandle ipc_handle;
  CHECK_DRV(
    cuMemExportToShareableHandle(
      (void *)&ipc_handle, original_handle,
      CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR, 0));

  // Create CUDA allocation and remap self
  CUdeviceptr d_addr;
  CHECK_DRV(cuMemAddressReserve(&d_addr, CUDA_RINGBUF_ALLOCATION_SIZE, 0, 0, 0ULL));

  // cuMemMap (with offset?)
  CHECK_DRV(cuMemMap(d_addr, pool_size, 0, original_handle, 0));

  // cuMemSetAccess
  CUmemAccessDesc accessDesc;
  accessDesc.location.id = 0;
  accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
  CHECK_DRV(cuMemSetAccess(d_addr, pool_size, &accessDesc, 1));

  // Free handle. Memory will stay valid as long as it is mapped
  CHECK_DRV(cuMemRelease(original_handle));

  struct cuda_ringbuf_allocator * alloc = (struct cuda_ringbuf_allocator *)create_shared_allocator(
    (void *)(uintptr_t)d_addr, sizeof(struct cuda_ringbuf_allocator), ALLOC_RING, CUDA, 0);

  // TODO: Construct strategy
  alloc->count = 0;
  alloc->rear_it = 0;
  alloc->item_size = item_size;
  alloc->ring_size = (pool_size - sizeof(struct cuda_ringbuf_allocator)) / item_size;
  alloc->ipc_handle = ipc_handle;

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

    // Give address relative to shared object
    int ret = sizeof(struct cuda_ringbuf_allocator) + s->item_size * forward_it;

    // Update count of how many elements in pool
    s->count++;

    return ret;
  }

  return 12345;
}

void cuda_ringbuf_deallocate(void * self, int offset)
{
  struct cuda_ringbuf_allocator * s = (struct cuda_ringbuf_allocator *)self;
  if (s->count == 0) {
    return;       // Allocator empty, nothing to deallocate
  }
  int entry = (offset - sizeof(struct cuda_ringbuf_allocator)) / s->item_size;

  // Do math with imaginary overflow indices so forward_it >= entry >= rear_it
  int forward_it = s->rear_it + s->count;
  if (__glibc_unlikely(entry < s->rear_it)) {
    entry += s->ring_size;
  }

  // Most likely scenario: entry == rear_it as allocations are deallocated in order
  s->rear_it = entry + 1;
  s->count = forward_it - s->rear_it;
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
  size_t pool_size = cuda_alloc->item_size * cuda_alloc->ring_size;

  // TODO: This is breaking on "invalid device ordinal". Something about passing a 0 to
  // cuMemCreate above. Look at this: https://github.com/NVIDIA/cuda-samples/blob/b312abaa07ffdc1ba6e3d44a9bc1a8e89149c20b/Samples/3_CUDA_Features/memMapIPCDrv/memMapIpc.cpp#L419
  // and investigate device IDs with cuDeviceGetCount, cuDeviceGet, etc
  // Get shareable handle
  CUmemGenericAllocationHandle handle;
  CHECK_DRV(
    cuMemImportFromShareableHandle(
      &handle, &(cuda_alloc->ipc_handle),
      CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR));

  // Create CUDA allocation and remap self
  CUdeviceptr d_addr;
  CHECK_DRV(cuMemAddressReserve(&d_addr, 0x80000000, 0, 0, 0ULL));

  // cuMemMap (with offset?)struct cpu_ringbuf_allocator * 
  CHECK_DRV(cuMemMap(d_addr, pool_size, 0, handle, 0));

  // cuMemSetAccess
  CUmemAccessDesc accessDesc;
  accessDesc.location.id = 0;
  accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
  CHECK_DRV(cuMemSetAccess(d_addr, pool_size, &accessDesc, 1));

  // Map in shared portion of allocator
  cuda_alloc = shmat(temp->shmem_id, (void *)(uintptr_t)d_addr, 0);

  // Free handle. Memory will stay valid as long as it is mapped
  CHECK_DRV(cuMemRelease(handle));

  // fps can now be typecast to cuda_ringbuf_allocator* and work correctly. Updates to any member
  // besides top 40 bytes will be visible across processes
  return cuda_alloc;
}

void cuda_ringbuf_unmap(struct hma_allocator * alloc)
{  
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

  CHECK_DRV(cuMemUnmap((CUdeviceptr)(uintptr_t)alloc, CUDA_RINGBUF_ALLOCATION_SIZE));
  CHECK_DRV(cuMemAddressFree((CUdeviceptr)(uintptr_t)alloc, CUDA_RINGBUF_ALLOCATION_SIZE));
}

#ifdef __cplusplus
}
#endif
