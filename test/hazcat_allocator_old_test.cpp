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

#include "rmw_hazcat_cpp/allocators/cpu_pool_allocator.hpp"
#include "rmw_hazcat_cpp/allocators/cuda_pool_allocator.hpp"
#include "rmw_hazcat_cpp/allocators/cpu_ringbuf_allocator.h"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

template<class T, size_t POOL_SIZE>
class TestWrapper : public StaticPoolAllocator<T, POOL_SIZE>
{
public:
  bool id_is(int truth)
  {
    return this->shmem_id == truth;
  }

  bool dealloc_is(void (* truth)(HMAAllocator<CPU_Mem> *, int))
  {
    return this->dealloc_fn == truth;
  }

  bool remap_is(void * (*truth)(void *))
  {
    return this->remap_fn == truth;
  }

  bool rear_it_is(int truth)
  {
    return this->rear_it == truth;
  }

  bool count_is(int truth)
  {
    return this->count == truth;
  }

  int pool_offset()
  {
    return (char *)&(this->pool) - (char *)this;
  }
};

template<class T, size_t POOL_SIZE>
class TestGPUWrapper : public StaticGPUPoolAllocator<T, POOL_SIZE>
{
public:
  bool id_is(int truth)
  {
    return this->shmem_id == truth;
  }

  bool dealloc_is(void (* truth)(HMAAllocator<CUDA_Mem> *, int))
  {
    return this->dealloc_fn == truth;
  }

  bool remap_is(void * (*truth)(void *))
  {
    return this->remap_fn == truth;
  }

  bool rear_it_is(int truth)
  {
    return this->rear_it == truth;
  }

  bool count_is(int truth)
  {
    return this->count == truth;
  }

  int pool_offset()
  {
    return (char *)&(this->pool) - (char *)this;
  }
};

TEST(AllocatorTest, creation_test)
{
  using AllocT = StaticPoolAllocator<test_msgs::msg::BasicTypes, 30>;
  auto cpu_alloc = (TestWrapper<test_msgs::msg::BasicTypes, 30> *) AllocT::create_shared_alloc();

  // Verify numbers mean what they should
  EXPECT_EQ(sizeof(int), 4UL);
  EXPECT_EQ(sizeof(long), 8UL);
  EXPECT_EQ(sizeof(void (*)(void *)), 8UL);

  // Verify alloc was created correctly
  EXPECT_NE(cpu_alloc, nullptr);

  // Verify function pointers are correct
  int id = cpu_alloc->get_id();
  EXPECT_TRUE(cpu_alloc->id_is(id));
  EXPECT_TRUE(cpu_alloc->dealloc_is(AllocT::static_deallocate));
  EXPECT_TRUE(cpu_alloc->remap_is(AllocT::static_remap));

  // Verify shared memory is destroyed correctly
  ((AllocT *)cpu_alloc)->~StaticPoolAllocator();
  EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
  EXPECT_EQ(errno, EINVAL);
}

// TEST(AllocatorTest, gpu_creation_test)
// {
//   CHECK_DRV(cuInit(0));

//   using AllocT = StaticGPUPoolAllocator<test_msgs::msg::BasicTypes, 30>;
//   auto cpu_alloc = (TestGPUWrapper<test_msgs::msg::BasicTypes, 30> *) AllocT::create_shared_alloc();

//   // Verify numbers mean what they should
//   EXPECT_EQ(sizeof(int), 4UL);
//   EXPECT_EQ(sizeof(long), 8UL);
//   EXPECT_EQ(sizeof(void (*)(void *)), 8UL);

//   // Verify alloc was created correctly
//   EXPECT_NE(cpu_alloc, nullptr);

//   // Verify function pointers are correct
//   int id = cpu_alloc->get_id();
//   EXPECT_TRUE(cpu_alloc->id_is(id));
//   EXPECT_TRUE(cpu_alloc->dealloc_is(AllocT::static_deallocate));
//   EXPECT_TRUE(cpu_alloc->remap_is(AllocT::static_remap));

//   // Verify shared memory is destroyed correctly
//   ((AllocT *)cpu_alloc)->~StaticGPUPoolAllocator();
//   EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
//   EXPECT_EQ(errno, EINVAL);
// }

TEST(AllocatorTest, allocate_rw_test)
{
  using AllocT = StaticPoolAllocator<std_msgs::msg::Int32, 3>;
  auto cpu_alloc = (TestWrapper<std_msgs::msg::Int32, 3> *) AllocT::create_shared_alloc();

  // First 3 allocations should go through, but the 4th should fail
  int a1 = cpu_alloc->allocate();
  EXPECT_TRUE(cpu_alloc->count_is(1));
  EXPECT_TRUE(cpu_alloc->rear_it_is(0));
  int a2 = cpu_alloc->allocate();
  EXPECT_TRUE(cpu_alloc->count_is(2));
  EXPECT_TRUE(cpu_alloc->rear_it_is(0));
  int a3 = cpu_alloc->allocate();
  EXPECT_TRUE(cpu_alloc->count_is(3));
  EXPECT_TRUE(cpu_alloc->rear_it_is(0));
  int a4 = cpu_alloc->allocate();
  EXPECT_TRUE(cpu_alloc->count_is(3));
  EXPECT_TRUE(cpu_alloc->rear_it_is(0));

  // Integer should be offset to pool array
  // TODO: Rewrite these with sizeof(type) operations, less hardcoded
  int pool_start = cpu_alloc->pool_offset();
  EXPECT_EQ(a1, pool_start);
  EXPECT_EQ(a2, pool_start + 4);
  EXPECT_EQ(a3, pool_start + 8);
  EXPECT_EQ(a4, -1);

  std_msgs::msg::Int32 * data1 = (std_msgs::msg::Int32 *)(cpu_alloc + a1);
  std_msgs::msg::Int32 * data2 = (std_msgs::msg::Int32 *)(cpu_alloc + a2);
  std_msgs::msg::Int32 * data3 = (std_msgs::msg::Int32 *)(cpu_alloc + a3);

  data1->data = 10;
  data2->data = 20;
  data3->data = 30;

  // Free first two places
  AllocT::static_deallocate(cpu_alloc, a1);
  EXPECT_TRUE(cpu_alloc->count_is(2));
  EXPECT_TRUE(cpu_alloc->rear_it_is(1));
  AllocT::static_deallocate(cpu_alloc, a2);
  EXPECT_TRUE(cpu_alloc->count_is(1));
  EXPECT_TRUE(cpu_alloc->rear_it_is(2));

  // New allocations should reoccupy these places
  int a5 = cpu_alloc->allocate();
  EXPECT_TRUE(cpu_alloc->count_is(2));
  EXPECT_TRUE(cpu_alloc->rear_it_is(2));
  int a6 = cpu_alloc->allocate();
  EXPECT_TRUE(cpu_alloc->count_is(3));
  EXPECT_TRUE(cpu_alloc->rear_it_is(2));
  EXPECT_EQ(a5, a1);
  EXPECT_EQ(a6, a2);


  std_msgs::msg::Int32 * data5 = (std_msgs::msg::Int32 *)(cpu_alloc + a5);
  std_msgs::msg::Int32 * data6 = (std_msgs::msg::Int32 *)(cpu_alloc + a6);

  // And data should be readable
  EXPECT_EQ(data5->data, 10);
  EXPECT_EQ(data6->data, 20);
  EXPECT_EQ(data3->data, 30);

  ((AllocT *)cpu_alloc)->~StaticPoolAllocator();

  //EXPECT_ANY_THROW(data5->data = 10);
}
