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

TEST(AllocatorTest, creation_test)
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