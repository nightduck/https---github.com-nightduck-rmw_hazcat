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

#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"

#include <gtest/gtest.h>

#include <string>
#include <tuple>
#include <vector>

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

TEST(AllocatorTest, struct_ordering_test) {
  EXPECT_EQ(offsetof(hma_allocator, shmem_id),      offsetof(cpu_ringbuf_allocator, shmem_id));
  EXPECT_EQ(offsetof(hma_allocator, strategy),      offsetof(cpu_ringbuf_allocator, strategy));
  EXPECT_EQ(offsetof(hma_allocator, device_type),   offsetof(cpu_ringbuf_allocator, device_type));
  EXPECT_EQ(offsetof(hma_allocator, device_number), offsetof(cpu_ringbuf_allocator, device_number));
  EXPECT_EQ(offsetof(hma_allocator, domain),        offsetof(cpu_ringbuf_allocator, device_type));
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
  EXPECT_EQ(a3 - a1, 16 + 2*sizeof(int));
  int a4 = ALLOCATE(alloc, 0);
  EXPECT_EQ(alloc->count, 3);
  EXPECT_EQ(alloc->rear_it, 0);
  EXPECT_EQ(a4, -1);

  // Assign data into these allocations
  long * data1 = GET_PTR(alloc, a1, long);
  long * data2 = GET_PTR(alloc, a2, long);
  long * data3 = GET_PTR(alloc, a3, long);
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
  long * data5 = GET_PTR(alloc, a5, long);
  long * data6 = GET_PTR(alloc, a6, long);

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
  long * a1_ptr = GET_PTR(alloc, a1, long);
  int * a1_ref = (int*)a1_ptr - 1;
  long * a2_ptr = GET_PTR(alloc, a2, long);
  int * a2_ref = (int*)a2_ptr - 1;
  long * a3_ptr = GET_PTR(alloc, a3, long);
  int * a3_ref = (int*)a3_ptr - 1;
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
  EXPECT_EQ(*a3_ref, 0);    // Allocator detect it's empty, so this ref counter doesn't get modified
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
  long * a1_ptr = GET_PTR(alloc, a1, long);
  int * a1_ref = (int*)a1_ptr - 1;
  long * a2_ptr = GET_PTR(alloc, a2, long);
  int * a2_ref = (int*)a2_ptr - 1;
  long * a3_ptr = GET_PTR(alloc, a3, long);
  int * a3_ref = (int*)a3_ptr - 1;
  EXPECT_EQ(*a1_ref, 1);
  EXPECT_EQ(*a2_ref, 1);
  EXPECT_EQ(*a3_ref, 1);

  hma_allocator_t * alloc2 = remap_shared_allocator(alloc->shmem_id);

  EXPECT_NE((void*)alloc, (void*)alloc2);

  // Contents of remapped allocator should be identical (local portion would differ across
  // processes, but these are in same process)
  int sz = (sizeof(struct cpu_ringbuf_allocator) + (8 + sizeof(int)) * 3)/sizeof(int);
  for(int i = 0; i < sz; i++) {
    EXPECT_EQ(((int*)alloc)[i], ((int*)alloc2)[i]);
  }

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