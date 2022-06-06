#ifdef __cplusplus
extern "C"
{
#endif

#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

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
  CHECK_DRV(cuMemAddressReserve(&d_addr, 0x80000000, 0, 0, 0ULL));

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
    (void *)(uintptr_t)d_addr, sizeof(struct cuda_ringbuf_allocator), CUDA, ALLOC_RING, 0);

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
    int ret = PTR_TO_OFFSET(s, sizeof(struct cuda_ringbuf_allocator) + s->item_size * forward_it);

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

void cuda_ringbuf_copy_from(void * there, void * here, size_t size)
{
  cudaMemcpy(there, here, size, cudaMemcpyHostToDevice);
}

void cuda_ringbuf_copy_to(void * there, void * here, size_t size)
{
  cudaMemcpy(here, there, size, cudaMemcpyDeviceToHost);
}

void cuda_ringbuf_copy(void * there, void * here, size_t size, struct hma_allocator * dest_alloc)
{
  void * interm = malloc(size);
  cuda_ringbuf_copy_to(interm, here, size);
  dest_alloc->copy_from(interm, there, size);
}

struct hma_allocator * cuda_ringbuf_remap(struct shared * temp)
{
  struct cuda_ringbuf_allocator * cuda_alloc =
    ALLOC_FROM_SHARED(temp, struct cuda_ringbuf_allocator);
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

  // cuMemMap (with offset?)
  CHECK_DRV(cuMemMap(d_addr, pool_size, 0, handle, 0));

  // cuMemSetAccess
  CUmemAccessDesc accessDesc;
  accessDesc.location.id = 0;
  accessDesc.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  accessDesc.flags = CU_MEM_ACCESS_FLAGS_PROT_READWRITE;
  CHECK_DRV(cuMemSetAccess(d_addr, pool_size, &accessDesc, 1));

  // Create a local mapping, and populate function pointers so they resolve in this process
  struct local * fps = (struct local *)mmap(
    (void *)(uintptr_t)d_addr,
    sizeof(struct local), PROT_READ | PROT_WRITE, MAP_FIXED | MAP_ANONYMOUS, 0, 0);
  populate_local_fn_pointers(fps, CUDA_RINGBUF_IMPL);

  // Map in shared portion of allocator
  shmat(temp->shmem_id, fps + sizeof(struct local), 0);

  // Free handle. Memory will stay valid as long as it is mapped
  CHECK_DRV(cuMemRelease(handle));

  // fps can now be typecast to cuda_ringbuf_allocator* and work correctly. Updates to any member
  // besides top 40 bytes will be visible across processes
  return (struct hma_allocator *)fps;
}

#ifdef __cplusplus
}
#endif
