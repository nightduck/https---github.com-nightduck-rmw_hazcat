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

#include "test_msgs/msg/bounded_sequences.hpp"
#include "std_msgs/msg/int32.hpp"

#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

#include <cuda_runtime_api.h>
#include <cuda.h>

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

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
#define CHECK(cudacall) { \
  int err=cudacall; \
  if (err!=cudaSuccess) \
    std::cout<<"CUDA ERROR "<<err<<" at line "<<__LINE__<<"'s "<<#cudacall<<"\n"; \
  }

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

__global__ void cuda_assert_eq(float* d_in, const float val) {
  printf("%f\n", *d_in);
  assert(*d_in == val);
}

TEST(AllocatorTest, struct_ordering_test) {
  EXPECT_EQ(offsetof(hma_allocator, shmem_id),      offsetof(cuda_ringbuf_allocator, shmem_id));
  EXPECT_EQ(offsetof(hma_allocator, strategy),      offsetof(cuda_ringbuf_allocator, strategy));
  EXPECT_EQ(offsetof(hma_allocator, device_type),   offsetof(cuda_ringbuf_allocator, device_type));
  EXPECT_EQ(offsetof(hma_allocator, device_number), offsetof(cuda_ringbuf_allocator, device_number));
  EXPECT_EQ(offsetof(hma_allocator, domain),        offsetof(cuda_ringbuf_allocator, device_type));
}

TEST(AllocatorTest, cuda_ringbuf_creation_test)
{
  CHECK_DRV(cuInit(0));
  struct cuda_ringbuf_allocator * alloc = create_cuda_ringbuf_allocator(6, 30);

  int id = alloc->shmem_id;
  EXPECT_EQ(alloc->strategy, ALLOC_RING);
  EXPECT_EQ(alloc->device_type, CUDA);
  EXPECT_EQ(alloc->device_number, 0);
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(alloc->item_size, 6);
  EXPECT_GE(alloc->ring_size, 30);
  
  unmap_shared_allocator((struct hma_allocator *)alloc);

  EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(AllocatorTest, cuda_ringbuf_allocate_rw_test)
{
  // Test allocations have following structure
  // struct test_thing {
  //   float header;
  //   uint8_t filler[min_cuda_allocation / 4];
  // };

  CUmemAllocationProp props = {};
  props.type = CU_MEM_ALLOCATION_TYPE_PINNED;
  props.location.type = CU_MEM_LOCATION_TYPE_DEVICE;
  props.location.id = 0;
  props.requestedHandleTypes = CU_MEM_HANDLE_TYPE_POSIX_FILE_DESCRIPTOR;
  size_t gran;
  CHECK_DRV(cuMemGetAllocationGranularity(&gran, &props, CU_MEM_ALLOC_GRANULARITY_MINIMUM));

  CHECK_DRV(cuInit(0));
  size_t allocation_size = sizeof(float) + gran/4;
  struct cuda_ringbuf_allocator * alloc = create_cuda_ringbuf_allocator(allocation_size, 3);

  // Make 4 allocations even though there's only room for 3
  int a1 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a1, SHARED_GRANULARITY + sizeof(fps_t));
  int a2 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a2 - a1, allocation_size);
  int a3 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a3 - a1, 2* allocation_size);
  int a4 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a4, -1);

  // Assign data into these allocations
  float * d_data1 = (float*)((uint8_t*)alloc + a1);
  float * d_data2 = (float*)((uint8_t*)alloc + a2);
  float * d_data3 = (float*)((uint8_t*)alloc + a3);
  float h_data1 = 4.5;
  float h_data2 = 2.25;
  float h_data3 = 1.125;
  COPY_TO(alloc, d_data1, &h_data1, sizeof(float));
  COPY_TO(alloc, d_data2, &h_data2, sizeof(float));
  COPY_TO(alloc, d_data3, &h_data3, sizeof(float));

  float hr_data1 = 0, hr_data2 = 0, hr_data3 = 0;
  COPY_FROM(alloc, d_data1, &hr_data1, sizeof(float));
  COPY_FROM(alloc, d_data2, &hr_data2, sizeof(float));
  COPY_FROM(alloc, d_data3, &hr_data3, sizeof(float));
  EXPECT_EQ(hr_data1, 4.5);
  EXPECT_EQ(hr_data2, 2.25);
  EXPECT_EQ(hr_data3, 1.125);

  // Deallocate two allocations
  DEALLOCATE(alloc, a1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  DEALLOCATE(alloc, a2);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);

  // New allocations should occupy those free spaces
  int a5 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 2);
  EXPECT_EQ(a5, a1);
  int a6 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 2);
  EXPECT_EQ(a6, a2);

  // Resolve pointers, but don't assign data, they should see old data (artificial leak)
  float * d_data5 = (float*)((uint8_t*)alloc + a5);
  float * d_data6 = (float*)((uint8_t*)alloc + a6);

  EXPECT_EQ(d_data5, d_data1);
  EXPECT_EQ(d_data6, d_data2);

  // And data should be readable (even from previous allocations)
  float hr_data5 = 0, hr_data6 = 0;
  COPY_FROM(alloc, d_data5, &hr_data5, sizeof(float));
  COPY_FROM(alloc, d_data6, &hr_data6, sizeof(float));
  EXPECT_EQ(hr_data5, 4.5);
  EXPECT_EQ(hr_data6, 2.25);

  unmap_shared_allocator((struct hma_allocator *)alloc);
}

TEST(AllocatorTest, cpu_ringbuf_share_deallocate_test)
{
  cuda_ringbuf_allocator_t * alloc = create_cuda_ringbuf_allocator(8, 3);
  int* ref_array = (int*)((uint8_t*)alloc + sizeof(cuda_ringbuf_allocator_t));
  
  // Make 3 allocations
  int a1 = ALLOCATE(alloc, 0);
  int a2 = ALLOCATE(alloc, 0);
  int a3 = ALLOCATE(alloc, 0);

  // Inspect reference counters which should all be 1
  EXPECT_EQ(ref_array[0], 1);
  EXPECT_EQ(ref_array[1], 1);
  EXPECT_EQ(ref_array[2], 1);

  // Share 2nd alloc once, and 3rd alloc twice
  SHARE(alloc, a2);
  EXPECT_EQ(ref_array[1], 2);
  SHARE(alloc, a3);
  EXPECT_EQ(ref_array[2], 2);
  SHARE(alloc, a3);
  EXPECT_EQ(ref_array[2], 3);

  // Initial state of allocator
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);

  // First allocation should only be deallocated once
  DEALLOCATE(alloc, a1);
  EXPECT_EQ(ref_array[0], 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  DEALLOCATE(alloc, a1);
  EXPECT_EQ(ref_array[0], -1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);

  DEALLOCATE(alloc, a2);
  EXPECT_EQ(ref_array[1], 1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  DEALLOCATE(alloc, a2);
  EXPECT_EQ(ref_array[1], 0);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);
  DEALLOCATE(alloc, a2);
  EXPECT_EQ(ref_array[1], -1);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);

  DEALLOCATE(alloc, a3);
  EXPECT_EQ(ref_array[2], 2);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);
  DEALLOCATE(alloc, a3);
  EXPECT_EQ(ref_array[2], 1);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);
  DEALLOCATE(alloc, a3);
  EXPECT_EQ(ref_array[2], 0);
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);
  DEALLOCATE(alloc, a3);
  EXPECT_EQ(ref_array[2], 0);    // Allocator detect it's empty, so this ref counter doesn't get modified
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);

  // Cleanup
  unmap_shared_allocator((struct hma_allocator *)alloc);
}

// TODO: Test copy method when an allocator from a different domain is written

TEST(AllocatorTest, cpu_ringbuf_remap_test)
{
  cuda_ringbuf_allocator_t * alloc = create_cuda_ringbuf_allocator(8, 3);
  int* ref_array = (int*)((uint8_t*)alloc + sizeof(cuda_ringbuf_allocator_t));

  // Make 3 allocations
  int a1 = ALLOCATE(alloc, 0);
  int a2 = ALLOCATE(alloc, 0);
  int a3 = ALLOCATE(alloc, 0);

  // Inspect reference counters which should all be 1
  EXPECT_EQ(ref_array[0], 1);
  EXPECT_EQ(ref_array[1], 1);
  EXPECT_EQ(ref_array[2], 1);

  hma_allocator_t * alloc2 = remap_shared_allocator(alloc->shmem_id);

  EXPECT_NE((void*)alloc, (void*)alloc2);

  // Contents of remapped allocator should be identical (local portion would differ across
  // processes, but these are in same process)
  int sz = (sizeof(cuda_ringbuf_allocator_t) + sizeof(int) * 3)/sizeof(int);
  for(int i = 0; i < sz; i++) {
    EXPECT_EQ(((int*)alloc)[i], ((int*)alloc2)[i]);
  }

  // TODO: Compare they have the same device boundary

  // Unmap initial allocator
  unmap_shared_allocator((hma_allocator_t*)alloc);

  // Allocator should still exist and be attachable
  void * temp = shmat(alloc2->shmem_id, NULL, 0);
  EXPECT_NE(temp, (void *)-1);
  EXPECT_EQ(shmdt(temp), 0);

  // Unmap 2nd allocator
  int id = alloc2->shmem_id;
  unmap_shared_allocator((hma_allocator_t*)alloc2);

  // Should no longer be able to attach
  EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
  EXPECT_EQ(errno, EINVAL);
}