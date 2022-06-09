// Copyright (c) 2020 by Robert Bosch GmbH. All rights reserved.
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

#include "test_msgs/msg/bounded_sequences.hpp"
#include "std_msgs/msg/int32.hpp"

#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"
#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

__global__ void cuda_assert_eq(float* d_in, const float val) {
  int gtid = blockIdx.x * blockDim.x + threadIdx.x;
  assert(*d_in == val);
}

TEST(AllocatorTest, cpu_ringbuf_creation_test)
{
  struct cpu_ringbuf_allocator * alloc = create_cpu_ringbuf_allocator(6, 30);

  int id = alloc->shmem_id;
  EXPECT_EQ(alloc->strategy, ALLOC_RING);
  EXPECT_EQ(alloc->device_type, CPU);
  EXPECT_EQ(alloc->device_number, 0);
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(alloc->item_size, 6);
  EXPECT_EQ(alloc->ring_size, 30);
  
  unmap_shared_allocator((struct hma_allocator *)alloc);

  EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(AllocatorTest, cuda_ringbuf_creation_test)
{
  struct cpu_ringbuf_allocator * alloc = create_cuda_ringbuf_allocator(6, 30);

  int id = alloc->shmem_id;
  EXPECT_EQ(alloc->strategy, ALLOC_RING);
  EXPECT_EQ(alloc->device_type, CUDA);
  EXPECT_EQ(alloc->device_number, 0);
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(alloc->item_size, 6);
  EXPECT_EQ(alloc->ring_size, 30);
  
  unmap_shared_allocator((struct hma_allocator *)alloc);

  EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(AllocatorTest, cpu_ringbuf_allocate_rw_test)
{
  struct cpu_ringbuf_allocator * alloc = create_cpu_ringbuf_allocator(8, 3);

  // Make 4 allocations even though there's only room for 3
  int a1 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a1, sizeof(cpu_ringbuf_allocator))
  int a2 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a2 - a1, 8);
  int a3 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a3 - a1, 16);
  int a4 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a4, -1);

  // Assign data into these allocations
  float * data1 = (float*)((uint8_t*)alloc + a1);
  float * data2 = (float*)((uint8_t*)alloc + a2);
  float * data3 = (float*)((uint8_t*)alloc + a3);
  *data1 = 1.234;
  *data2 = 2.468;
  *data3 = 4.936;

  // Deallocate two allocations
  deallocate(alloc, a1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  deallocate(alloc, a2);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);

  // New allocations should occupy those free spaces
  int a3 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 2);
  EXPECT_EQ(a5, a1);
  int a4 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 2);
  EXPECT_EQ(a6, a2);

  // Assign data into these allocations
  float * data5 = (float*)((uint8_t*)alloc + a5);
  float * data6 = (float*)((uint8_t*)alloc + a6);

  // And data should be readable (even from previous allocations)
  EXPECT_EQ(*data5, 1.234);
  EXPECT_EQ(*data6, 2.468);
  EXPECT_EQ(*data3, 4.936);

  unmap_shared_allocator((struct hma_allocator *)alloc);
}

TEST(AllocatorTest, cuda_ringbuf_allocate_rw_test)
{
  struct cuda_ringbuf_allocator * alloc = create_cuda_ringbuf_allocator(8, 3);

  // Make 4 allocations even though there's only room for 3
  int a1 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a1, sizeof(cpu_ringbuf_allocator))
  int a2 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a2 - a1, 8);
  int a3 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a3 - a1, 16);
  int a4 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a4, -1);

  // Assign data into these allocations
  float * d_data1 = (float*)((uint8_t*)alloc + a1);
  float * d_data2 = (float*)((uint8_t*)alloc + a2);
  float * d_data3 = (float*)((uint8_t*)alloc + a3);
  float h_data1 = 1.234;
  float h_data2 = 2.468;
  float h_data3 = 4.936;
  cudaMemcpy(d_data1, h_data1, sizeof(float) cudaMemCpyHostToDevice);
  cudaMemcpy(d_data2, h_data2, sizeof(float) cudaMemCpyHostToDevice);
  cudaMemcpy(d_data3, h_data3, sizeof(float) cudaMemCpyHostToDevice);

  // Deallocate two allocations
  deallocate(alloc, a1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  deallocate(alloc, a2);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);

  // New allocations should occupy those free spaces
  int a3 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 2);
  EXPECT_EQ(a5, a1);
  int a4 = allocate(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 2);
  EXPECT_EQ(a6, a2);

  // Assign data into these allocations
  float * d_data5 = (float*)((uint8_t*)alloc + a5);
  float * d_data6 = (float*)((uint8_t*)alloc + a6);

  // And data should be readable (even from previous allocations)
  cuda_assert_eq(d_data5, 1.234);
  cuda_assert_eq(d_data6, 2.468);
  cuda_assert_eq(d_data3, 4.936);

  unmap_shared_allocator((struct hma_allocator *)alloc);
}