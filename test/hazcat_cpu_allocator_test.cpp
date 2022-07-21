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

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

#include "test_msgs/msg/bounded_sequences.hpp"
#include "std_msgs/msg/int32.hpp"

#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

TEST(AllocatorTest, struct_ordering_test) {
  using alloc_type = cpu_ringbuf_allocator;
  EXPECT_EQ(offsetof(hma_allocator, shmem_id), offsetof(alloc_type, shmem_id));
  EXPECT_EQ(offsetof(hma_allocator, strategy), offsetof(alloc_type, strategy));
  EXPECT_EQ(offsetof(hma_allocator, device_type), offsetof(alloc_type, device_type));
  EXPECT_EQ(offsetof(hma_allocator, device_number), offsetof(alloc_type, device_number));
  EXPECT_EQ(offsetof(hma_allocator, domain), offsetof(alloc_type, device_type));
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

TEST(AllocatorTest, cpu_ringbuf_allocate_rw_test)
{
  struct cpu_ringbuf_allocator * alloc = create_cpu_ringbuf_allocator(8, 3);

  // Make 4 allocations even though there's only room for 3
  int a1 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a1, sizeof(cpu_ringbuf_allocator) + sizeof(int));
  int a2 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a2 - a1, 8 + sizeof(int));
  int a3 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a3 - a1, 16 + 2 * sizeof(int));
  int a4 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a4, -1);

  // Assign data into these allocations
  int64_t * data1 = GET_PTR(alloc, a1, int64_t);
  int64_t * data2 = GET_PTR(alloc, a2, int64_t);
  int64_t * data3 = GET_PTR(alloc, a3, int64_t);
  *data1 = 3875;
  *data2 = 5878;
  *data3 = 109;

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

  // Assign data into these allocations
  int64_t * data5 = GET_PTR(alloc, a5, int64_t);
  int64_t * data6 = GET_PTR(alloc, a6, int64_t);

  // And data should be readable (even from previous allocations)
  EXPECT_EQ(*data5, 3875);
  EXPECT_EQ(*data6, 5878);
  EXPECT_EQ(*data3, 109);

  unmap_shared_allocator((struct hma_allocator *)alloc);
}

TEST(AllocatorTest, cpu_ringbuf_share_deallocate_test)
{
  struct cpu_ringbuf_allocator * alloc = create_cpu_ringbuf_allocator(8, 3);

  // Make 3 allocations
  int a1 = ALLOCATE(alloc, 0);
  int a2 = ALLOCATE(alloc, 0);
  int a3 = ALLOCATE(alloc, 0);

  // Inspect reference counters which should all be 1
  int64_t * a1_ptr = GET_PTR(alloc, a1, int64_t);
  int * a1_ref = reinterpret_cast<int *>(a1_ptr) - 1;
  int64_t * a2_ptr = GET_PTR(alloc, a2, int64_t);
  int * a2_ref = reinterpret_cast<int *>(a2_ptr) - 1;
  int64_t * a3_ptr = GET_PTR(alloc, a3, int64_t);
  int * a3_ref = reinterpret_cast<int *>(a3_ptr) - 1;
  EXPECT_EQ(*a1_ref, 1);
  EXPECT_EQ(*a2_ref, 1);
  EXPECT_EQ(*a3_ref, 1);

  // Share 2nd alloc once, and 3rd alloc twice
  SHARE(alloc, a2);
  EXPECT_EQ(*a2_ref, 2);
  SHARE(alloc, a3);
  EXPECT_EQ(*a3_ref, 2);
  SHARE(alloc, a3);
  EXPECT_EQ(*a3_ref, 3);

  // Initial state of allocator
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);

  // First allocation should only be deallocated once
  DEALLOCATE(alloc, a1);
  EXPECT_EQ(*a1_ref, 0);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  DEALLOCATE(alloc, a1);
  EXPECT_EQ(*a1_ref, -1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);

  DEALLOCATE(alloc, a2);
  EXPECT_EQ(*a2_ref, 1);
  EXPECT_EQ(alloc->count, 2);
  EXPECT_EQ(alloc->rear_it, 1);
  DEALLOCATE(alloc, a2);
  EXPECT_EQ(*a2_ref, 0);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);
  DEALLOCATE(alloc, a2);
  EXPECT_EQ(*a2_ref, -1);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);

  DEALLOCATE(alloc, a3);
  EXPECT_EQ(*a3_ref, 2);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);
  DEALLOCATE(alloc, a3);
  EXPECT_EQ(*a3_ref, 1);
  EXPECT_EQ(alloc->count, 1);
  EXPECT_EQ(alloc->rear_it, 2);
  DEALLOCATE(alloc, a3);
  EXPECT_EQ(*a3_ref, 0);
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);
  DEALLOCATE(alloc, a3);
  EXPECT_EQ(*a3_ref, 0);
  EXPECT_EQ(alloc->count, 0);
  EXPECT_EQ(alloc->rear_it, 0);

  // Cleanup
  unmap_shared_allocator((struct hma_allocator *)alloc);
}

// TEST(AllocatorTest, cpu_ringbuf_copy_test)
// {
// }

TEST(AllocatorTest, cpu_ringbuf_remap_test)
{
  struct cpu_ringbuf_allocator * alloc = create_cpu_ringbuf_allocator(8, 3);

  // Make 3 allocations
  int a1 = ALLOCATE(alloc, 0);
  int a2 = ALLOCATE(alloc, 0);
  int a3 = ALLOCATE(alloc, 0);

  // Inspect reference counters which should all be 1
  int64_t * a1_ptr = GET_PTR(alloc, a1, int64_t);
  int * a1_ref = reinterpret_cast<int *>(a1_ptr) - 1;
  int64_t * a2_ptr = GET_PTR(alloc, a2, int64_t);
  int * a2_ref = reinterpret_cast<int *>(a2_ptr) - 1;
  int64_t * a3_ptr = GET_PTR(alloc, a3, int64_t);
  int * a3_ref = reinterpret_cast<int *>(a3_ptr) - 1;
  EXPECT_EQ(*a1_ref, 1);
  EXPECT_EQ(*a2_ref, 1);
  EXPECT_EQ(*a3_ref, 1);

  hma_allocator_t * alloc2 = remap_shared_allocator(alloc->shmem_id);

  EXPECT_NE((void *)alloc, (void *)alloc2);

  // Contents of remapped allocator should be identical (local portion would differ across
  // processes, but these are in same process)
  int sz = (sizeof(struct cpu_ringbuf_allocator) + (8 + sizeof(int)) * 3) / sizeof(int);
  for (int i = 0; i < sz; i++) {
    EXPECT_EQ(reinterpret_cast<int *>(alloc)[i], reinterpret_cast<int *>(alloc2)[i]);
  }

  // Unmap initial allocator
  unmap_shared_allocator(reinterpret_cast<hma_allocator_t *>(alloc));

  // Allocator should still exist and be attachable
  void * temp = shmat(alloc2->shmem_id, NULL, 0);
  EXPECT_NE(temp, (void *)-1);
  EXPECT_EQ(shmdt(temp), 0);

  // Unmap 2nd allocator
  int id = alloc2->shmem_id;
  unmap_shared_allocator(reinterpret_cast<hma_allocator_t *>(alloc2));

  // Should no int64_ter be able to attach
  EXPECT_EQ(shmat(id, NULL, 0), (void *)-1);
  EXPECT_EQ(errno, EINVAL);
}
