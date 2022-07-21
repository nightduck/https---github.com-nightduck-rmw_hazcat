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

#include <cuda_runtime_api.h>
#include <cuda.h>

#include <gtest/gtest.h>

#include <dirent.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <tuple>
#include <vector>

#include "test_msgs/msg/bounded_sequences.hpp"
#include "std_msgs/msg/int32.hpp"

#include "rmw_hazcat/allocators/cpu_ringbuf_allocator.h"
#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"
#include "rmw_hazcat/hazcat_message_queue.h"
#include "rmw_hazcat/hashtable.h"

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

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

inline ref_bits_t * get_ref_bits(message_queue_t * mq, int i)
{
  return reinterpret_cast<ref_bits_t *>(reinterpret_cast<uint8_t *>(mq) +
         sizeof(message_queue_t) + i * sizeof(ref_bits_t));
}

inline entry_t * get_entry(message_queue_t * mq, int domain, int i)
{
  return reinterpret_cast<entry_t *>(reinterpret_cast<uint8_t *>(mq) + sizeof(message_queue_t) +
         mq->len * sizeof(ref_bits_t) + domain * mq->len * sizeof(entry_t) + i * sizeof(entry_t));
}

class MessageQueueTest : public testing::Test
{
public:
  mq_node_t * mq_node;
  rmw_publisher_t * cpu_pub;
  rmw_publisher_t * cuda_pub;
  rmw_subscription_t * cpu_sub;
  rmw_subscription_t * cpu_sub2;
  rmw_subscription_t * cuda_sub;
  cpu_ringbuf_allocator_t * cpu_alloc;
  cuda_ringbuf_allocator_t * cuda_alloc;
  rmw_qos_profile_t pub_qos, sub_qos;
};

TEST(HashtableTest, hashtable_test) {
  // Creation test
  hashtable_t * ht = hashtable_init(8);
  EXPECT_EQ(ht->len, 8);
  EXPECT_EQ(ht->count, 0);
  EXPECT_EQ(
    reinterpret_cast<uint8_t *>(ht->table), reinterpret_cast<uint8_t *>(ht) +
    sizeof(hashtable_t));
  for (int i = 0; i < 8; i++) {
    EXPECT_EQ(ht->table[i].next, nullptr);
    EXPECT_EQ(ht->table[i].val, nullptr);
  }

  // Attempt retrival of non-existant element
  EXPECT_EQ(hashtable_get(ht, 42), nullptr);

  // Ordinary insertion and removal test
  hashtable_insert(ht, 42, reinterpret_cast<void *>(0x42));
  EXPECT_EQ(ht->table[HASH(42, 8)].val, reinterpret_cast<void *>(0x42));
  EXPECT_EQ(ht->table[HASH(42, 8)].key, 42);
  EXPECT_EQ(ht->table[HASH(42, 8)].next, nullptr);
  EXPECT_EQ(hashtable_get(ht, 42), reinterpret_cast<void *>(0x42));
  hashtable_remove(ht, 42);
  EXPECT_EQ(ht->table[HASH(42, 8)].val, nullptr);
  EXPECT_EQ(ht->table[HASH(42, 8)].next, nullptr);
  EXPECT_EQ(hashtable_get(ht, 42), nullptr);

  // Simple collision test
  EXPECT_EQ(HASH(0x11, 8), 6);
  EXPECT_EQ(HASH(0x21, 8), 6);
  EXPECT_EQ(HASH(0x31, 8), 6);
  hashtable_insert(ht, 0x11, reinterpret_cast<void *>(0x11));  // Insert into 2nd from last slot
  hashtable_insert(ht, 0x21, reinterpret_cast<void *>(0x21));  // Collide and land in last slot
  hashtable_insert(ht, 0x31, reinterpret_cast<void *>(0x31));  // Collide and land in first slot
  EXPECT_EQ(ht->table[6].val, reinterpret_cast<void *>(0x11));
  EXPECT_EQ(ht->table[7].val, reinterpret_cast<void *>(0x21));
  EXPECT_EQ(ht->table[0].val, reinterpret_cast<void *>(0x31));
  EXPECT_EQ(ht->table[6].key, 0x11);
  EXPECT_EQ(ht->table[7].key, 0x21);
  EXPECT_EQ(ht->table[0].key, 0x31);
  EXPECT_EQ(ht->table[6].next, &(ht->table[7]));
  EXPECT_EQ(ht->table[7].next, &(ht->table[0]));
  EXPECT_EQ(ht->table[0].next, nullptr);

  // Removal test
  // TODO(nightduck): (remove 0x21 from above)
  hashtable_remove(ht, 0x21);
  EXPECT_EQ(ht->table[6].val, reinterpret_cast<void *>(0x11));
  EXPECT_EQ(ht->table[0].val, reinterpret_cast<void *>(0x31));
  EXPECT_EQ(ht->table[6].key, 0x11);
  EXPECT_EQ(ht->table[0].key, 0x31);
  EXPECT_EQ(ht->table[6].next, &(ht->table[0]));
  EXPECT_EQ(ht->table[0].next, nullptr);


  // Collision between non matching hashes, requires relocating some entries and rewriting lists
  EXPECT_EQ(HASH(0x17, 8), 0);
  EXPECT_EQ(HASH(0x27, 8), 0);
  hashtable_insert(ht, 0x21, reinterpret_cast<void *>(0x21));
  hashtable_insert(ht, 0x17, reinterpret_cast<void *>(0x17));
  hashtable_insert(ht, 0x27, reinterpret_cast<void *>(0x27));
  EXPECT_EQ(ht->table[0].val, reinterpret_cast<void *>(0x17));
  EXPECT_EQ(ht->table[1].val, reinterpret_cast<void *>(0x21));
  EXPECT_EQ(ht->table[2].val, reinterpret_cast<void *>(0x31));
  EXPECT_EQ(ht->table[3].val, reinterpret_cast<void *>(0x27));
  EXPECT_EQ(ht->table[6].val, reinterpret_cast<void *>(0x11));
  EXPECT_EQ(ht->table[0].key, 0x17);
  EXPECT_EQ(ht->table[1].key, 0x21);
  EXPECT_EQ(ht->table[2].key, 0x31);
  EXPECT_EQ(ht->table[3].key, 0x27);
  EXPECT_EQ(ht->table[6].key, 0x11);
  EXPECT_EQ(ht->table[0].next, &(ht->table[3]));
  EXPECT_EQ(ht->table[1].next, nullptr);
  EXPECT_EQ(ht->table[2].next, &(ht->table[1]));
  EXPECT_EQ(ht->table[3].next, nullptr);
  EXPECT_EQ(ht->table[6].next, &(ht->table[2]));

  // Removal test: remove head of list
  hashtable_remove(ht, 0x17);
  EXPECT_EQ(ht->table[0].val, reinterpret_cast<void *>(0x27));
  EXPECT_EQ(ht->table[1].val, reinterpret_cast<void *>(0x21));
  EXPECT_EQ(ht->table[2].val, reinterpret_cast<void *>(0x31));
  EXPECT_EQ(ht->table[3].val, nullptr);
  EXPECT_EQ(ht->table[6].val, reinterpret_cast<void *>(0x11));
  EXPECT_EQ(ht->table[0].key, 0x27);
  EXPECT_EQ(ht->table[1].key, 0x21);
  EXPECT_EQ(ht->table[2].key, 0x31);
  EXPECT_EQ(ht->table[6].key, 0x11);
  EXPECT_EQ(ht->table[0].next, nullptr);
  EXPECT_EQ(ht->table[1].next, nullptr);
  EXPECT_EQ(ht->table[2].next, &(ht->table[1]));
  EXPECT_EQ(ht->table[3].next, nullptr);
  EXPECT_EQ(ht->table[6].next, &(ht->table[2]));

  // Overwrite test
  // Insert 0x11 again, but with new value
  hashtable_insert(ht, 0x11, reinterpret_cast<void *>(0x1234));
  EXPECT_EQ(ht->table[1].val, reinterpret_cast<void *>(0x21));
  EXPECT_EQ(ht->table[2].val, reinterpret_cast<void *>(0x31));
  EXPECT_EQ(ht->table[6].val, reinterpret_cast<void *>(0x1234));
  EXPECT_EQ(ht->table[1].key, 0x21);
  EXPECT_EQ(ht->table[2].key, 0x31);
  EXPECT_EQ(ht->table[6].key, 0x11);
  EXPECT_EQ(ht->table[1].next, nullptr);
  EXPECT_EQ(ht->table[2].next, &(ht->table[1]));
  EXPECT_EQ(ht->table[6].next, &(ht->table[2]));

  hashtable_fini(ht);
}

// Not a test, just resets
TEST_F(MessageQueueTest, setup) {
  CHECK_DRV(cuInit(0));  // Initialize CUDA driver

  DIR * d;
  struct dirent * dir;
  d = opendir("/dev/shm/");
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (strncmp(dir->d_name, "ros2_", 5) == 0) {
        int ret = shm_unlink(dir->d_name);
        EXPECT_EQ(ret, 0);
      }
    }
    closedir(d);
  }
}

TEST_F(MessageQueueTest, creation_and_registration) {
  rmw_node_t dummy;                                     // Content doesn't matter
  rosidl_message_type_support_t dummy_type_support;     // Content doesn't matter

  cpu_pub = rmw_publisher_allocate();
  cpu_sub = rmw_subscription_allocate();
  pub_sub_data_t * pub_data = reinterpret_cast<pub_sub_data_t *>(
    rmw_allocate(sizeof(pub_sub_data_t)));
  pub_sub_data_t * sub_data = reinterpret_cast<pub_sub_data_t *>(
    rmw_allocate(sizeof(pub_sub_data_t)));

  // Populate data->alloc with allocator
  cpu_alloc = create_cpu_ringbuf_allocator(8, 10);
  pub_data->alloc = reinterpret_cast<hma_allocator_t *>(cpu_alloc);
  sub_data->alloc = reinterpret_cast<hma_allocator_t *>(cpu_alloc);
  pub_data->depth = 5;
  sub_data->depth = 1;

  cpu_pub->implementation_identifier = rmw_get_implementation_identifier();
  cpu_pub->data = pub_data;
  cpu_pub->topic_name = "test";
  cpu_pub->can_loan_messages = true;

  cpu_sub->implementation_identifier = rmw_get_implementation_identifier();
  cpu_sub->data = sub_data;
  cpu_sub->topic_name = "test";
  cpu_sub->can_loan_messages = true;

  ASSERT_EQ(hazcat_init(), RMW_RET_OK);

  ASSERT_EQ(hazcat_register_publisher(cpu_pub), RMW_RET_OK);

  mq_node = pub_data->mq;
  message_queue_t * mq = mq_node->elem;
  EXPECT_STREQ(mq_node->file_name, "/ros2_hazcat.test");
  ASSERT_GT(mq_node->fd, 0);

  EXPECT_EQ(mq->index, 0);
  EXPECT_EQ(mq->len, 5);
  EXPECT_EQ(mq->num_domains, 1);
  EXPECT_EQ(mq->domains[0], cpu_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 0);

  ASSERT_EQ(hazcat_register_subscription(cpu_sub), RMW_RET_OK);

  EXPECT_EQ(mq, sub_data->mq->elem);    // Should use same message queue
  EXPECT_EQ(mq_node, sub_data->mq);
  EXPECT_EQ(mq->index, 0);
  EXPECT_EQ(mq->len, 5);
  EXPECT_EQ(mq->num_domains, 1);
  EXPECT_EQ(mq->domains[0], cpu_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 1);

  // Test pub and sub data
  EXPECT_EQ(pub_data->alloc, &cpu_alloc->untyped);    // Should reference allocator
  EXPECT_EQ(sub_data->alloc, &cpu_alloc->untyped);
  EXPECT_EQ(pub_data->next_index, 0);   // Should be waiting at front of message queue
  EXPECT_EQ(sub_data->next_index, 0);
  EXPECT_EQ(pub_data->array_num, 0);    // Should use same first domain
  EXPECT_EQ(sub_data->array_num, 0);
}

TEST_F(MessageQueueTest, basic_rw) {
  message_queue_t * mq = mq_node->elem;

  // Test take, should expect empty message
  msg_ref_t msg_ref = hazcat_take(cpu_sub);
  EXPECT_EQ(msg_ref.alloc, nullptr);
  EXPECT_EQ(msg_ref.msg, nullptr);

  // Publish 2 messages
  int msg1_offset = ALLOCATE(cpu_alloc, 8);
  int64_t * msg1 = GET_PTR(cpu_alloc, msg1_offset, int64_t);
  int msg2_offset = ALLOCATE(cpu_alloc, 8);
  int64_t * msg2 = GET_PTR(cpu_alloc, msg2_offset, int64_t);
  ASSERT_EQ(hazcat_publish(cpu_pub, msg1, 8), RMW_RET_OK);
  EXPECT_EQ(mq->index, 1);
  ref_bits_t * ref_bits = get_ref_bits(mq, 0);
  entry_t * entry = get_entry(mq, 0, 0);
  EXPECT_EQ(ref_bits->availability, 0x1);
  EXPECT_EQ(ref_bits->interest_count, 1);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(entry->offset, msg1_offset);
  ASSERT_EQ(hazcat_publish(cpu_pub, msg2, 8), RMW_RET_OK);
  EXPECT_EQ(mq->index, 2);
  ref_bits = get_ref_bits(mq, 1);
  entry = get_entry(mq, 0, 1);
  EXPECT_EQ(ref_bits->availability, 0x1);
  EXPECT_EQ(ref_bits->interest_count, 1);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(entry->offset, msg2_offset);

  // Test take, should only receive most recent message
  msg_ref = hazcat_take(cpu_sub);
  EXPECT_EQ(msg_ref.msg, msg2);
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cpu_alloc));
  EXPECT_EQ(ref_bits->availability, 0x1);
  EXPECT_EQ(ref_bits->interest_count, 0);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(entry->offset, msg2_offset);

  // Test take, should expect empty message
  msg_ref = hazcat_take(cpu_sub);
  EXPECT_EQ(msg_ref.alloc, nullptr);
  EXPECT_EQ(msg_ref.msg, nullptr);
}

TEST_F(MessageQueueTest, multi_domain_registration) {
  cpu_sub2 = rmw_subscription_allocate();
  cuda_sub = rmw_subscription_allocate();
  cuda_pub = rmw_publisher_allocate();

  pub_sub_data_t * cpu_sub_data = reinterpret_cast<pub_sub_data_t *>(
    rmw_allocate(sizeof(pub_sub_data_t)));
  pub_sub_data_t * cuda_sub_data = reinterpret_cast<pub_sub_data_t *>(
    rmw_allocate(sizeof(pub_sub_data_t)));
  pub_sub_data_t * pub_data = reinterpret_cast<pub_sub_data_t *>(
    rmw_allocate(sizeof(pub_sub_data_t)));

  // Populate subscriptions
  cuda_alloc = create_cuda_ringbuf_allocator(8, 10);
  pub_data->alloc = reinterpret_cast<hma_allocator_t *>(cuda_alloc);
  cuda_sub_data->alloc = reinterpret_cast<hma_allocator_t *>(cuda_alloc);
  cpu_sub_data->alloc = reinterpret_cast<hma_allocator_t *>(cpu_alloc);
  pub_data->depth = 5;
  cpu_sub_data->depth = 10;
  cuda_sub_data->depth = 10;

  cuda_pub->implementation_identifier = rmw_get_implementation_identifier();
  cuda_pub->data = pub_data;
  cuda_pub->topic_name = "test";
  cuda_pub->can_loan_messages = true;

  cpu_sub2->implementation_identifier = rmw_get_implementation_identifier();
  cpu_sub2->data = cpu_sub_data;
  cpu_sub2->topic_name = "test";
  cpu_sub2->can_loan_messages = true;

  cuda_sub->implementation_identifier = rmw_get_implementation_identifier();
  cuda_sub->data = cuda_sub_data;
  cuda_sub->topic_name = "test";
  cuda_sub->can_loan_messages = true;

  message_queue_t * mq = mq_node->elem;

  ASSERT_EQ(hazcat_register_subscription(cuda_sub), RMW_RET_OK);
  EXPECT_EQ(mq_node, cuda_sub_data->mq);
  EXPECT_EQ(mq, cuda_sub_data->mq->elem);
  EXPECT_EQ(mq->index, 2);
  EXPECT_EQ(mq->len, 10);
  EXPECT_EQ(mq->num_domains, 2);
  EXPECT_EQ(mq->domains[1], cuda_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 2);

  ASSERT_EQ(hazcat_register_publisher(cuda_pub), RMW_RET_OK);
  EXPECT_EQ(mq_node, pub_data->mq);
  EXPECT_EQ(mq, pub_data->mq->elem);
  EXPECT_EQ(mq->index, 2);
  EXPECT_EQ(mq->len, 10);
  EXPECT_EQ(mq->num_domains, 2);
  EXPECT_EQ(mq->domains[1], cuda_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 2);
  EXPECT_EQ(mq->sub_count, 2);


  ASSERT_EQ(hazcat_register_subscription(cpu_sub2), RMW_RET_OK);
  EXPECT_EQ(mq, cpu_sub_data->mq->elem);    // Should use same message queue
  EXPECT_EQ(mq_node, cpu_sub_data->mq);
  EXPECT_EQ(mq->index, 2);
  EXPECT_EQ(mq->len, 10);
  EXPECT_EQ(mq->num_domains, 2);
  EXPECT_EQ(mq->domains[0], cpu_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 2);
  EXPECT_EQ(mq->sub_count, 3);

  // Test pub and sub data
  EXPECT_EQ(pub_data->alloc, &cuda_alloc->untyped);    // Should reference allocator
  EXPECT_EQ(cuda_sub_data->alloc, &cuda_alloc->untyped);
  EXPECT_EQ(cpu_sub_data->alloc, &cpu_alloc->untyped);
  EXPECT_EQ(cuda_sub_data->next_index, 2);  // Should be waiting at front of message queue
  EXPECT_EQ(cpu_sub_data->next_index, 2);
  EXPECT_EQ(pub_data->array_num, 1);    // Should use same first domain
  EXPECT_EQ(cuda_sub_data->array_num, 1);
  EXPECT_EQ(cpu_sub_data->array_num, 0);
}

TEST_F(MessageQueueTest, multi_domain_rw) {
  message_queue_t * mq = mq_node->elem;
  ref_bits_t * ref_bits;
  entry_t * entry;
  msg_ref_t msg_ref;

  // Publish on CPU
  int msg1_offset = ALLOCATE(cpu_alloc, 8);
  int64_t * msg1 = GET_PTR(cpu_alloc, msg1_offset, int64_t);
  *msg1 = 0xDEADBEEF;
  ASSERT_EQ(hazcat_publish(cpu_pub, msg1, 8), RMW_RET_OK);
  EXPECT_EQ(mq->index, 3);
  ref_bits = get_ref_bits(mq, 2);
  entry = get_entry(mq, 0, 2);
  EXPECT_EQ(ref_bits->availability, 0x1);
  EXPECT_EQ(ref_bits->interest_count, 3);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(entry->offset, msg1_offset);

  // Read on CPU, verify message is same
  msg_ref = hazcat_take(cpu_sub2);
  EXPECT_EQ(msg_ref.msg, msg1);
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cpu_alloc));
  EXPECT_EQ(ref_bits->availability, 0x1);
  EXPECT_EQ(ref_bits->interest_count, 2);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(entry->offset, msg1_offset);

  // Read on GPU sub
  msg_ref = hazcat_take(cuda_sub);
  EXPECT_NE(msg_ref.msg, msg1);
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cuda_alloc));
  EXPECT_EQ(ref_bits->availability, 0x3);
  EXPECT_EQ(ref_bits->interest_count, 1);
  EXPECT_EQ(ref_bits->lock, 0);
  entry = get_entry(mq, 1, 2);
  EXPECT_EQ(entry->alloc_shmem_id, cuda_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);

  // Take same message and republish on GPU pub
  int msg2_offset = reinterpret_cast<uint8_t *>(msg_ref.msg) -
    reinterpret_cast<uint8_t *>(msg_ref.alloc);
  int64_t * msg2 = reinterpret_cast<int64_t *>(msg_ref.msg);
  ASSERT_EQ(hazcat_publish(cuda_pub, msg2, 8), RMW_RET_OK);
  EXPECT_EQ(mq->index, 4);
  ref_bits = get_ref_bits(mq, 3);
  entry = get_entry(mq, 1, 3);
  EXPECT_EQ(ref_bits->availability, 0x2);
  EXPECT_EQ(ref_bits->interest_count, 3);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cuda_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(entry->offset, msg2_offset);

  // Read on GPU sub, message addr should match, but be at separate entry in message queue
  msg_ref = hazcat_take(cuda_sub);
  EXPECT_EQ(msg_ref.msg, msg2);
  EXPECT_NE(msg_ref.msg, msg1);
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cuda_alloc));
  EXPECT_EQ(ref_bits->availability, 0x2);
  EXPECT_EQ(ref_bits->interest_count, 2);
  EXPECT_EQ(ref_bits->lock, 0);
  EXPECT_EQ(entry->alloc_shmem_id, cuda_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);

  // Read on goldfish memory sub, verify correct entry was read and copied over
  msg_ref = hazcat_take(cpu_sub);
  int64_t * cpu_msg2 = reinterpret_cast<int64_t *>(msg_ref.msg);
  EXPECT_EQ(*cpu_msg2, *msg1);
  EXPECT_NE(cpu_msg2, msg2);
  EXPECT_NE(cpu_msg2, msg1);
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cpu_alloc));
  EXPECT_EQ(ref_bits->availability, 0x3);
  EXPECT_EQ(ref_bits->interest_count, 1);
  EXPECT_EQ(ref_bits->lock, 0);
  entry = get_entry(mq, 0, 3);
  EXPECT_EQ(entry->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry->len, 8);
  EXPECT_EQ(
    reinterpret_cast<uint8_t *>(cpu_alloc) + entry->offset,
    reinterpret_cast<uint8_t *>(cpu_msg2));

  // Read on other CPU sub, message addr should match previous sub read
  msg_ref = hazcat_take(cpu_sub2);
  EXPECT_EQ(msg_ref.msg, reinterpret_cast<void *>(cpu_msg2));
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cpu_alloc));
  EXPECT_EQ(ref_bits->availability, 0x3);
  EXPECT_EQ(ref_bits->interest_count, 0);
  EXPECT_EQ(ref_bits->lock, 0);
}

TEST_F(MessageQueueTest, unregister_cuda) {
  message_queue_t * mq = MessageQueueTest::mq_node->elem;
  rmw_publisher_t * cuda_pub = MessageQueueTest::cuda_pub;
  rmw_subscription_t * cuda_sub = MessageQueueTest::cuda_sub;

  EXPECT_EQ(hazcat_unregister_publisher(cuda_pub), RMW_RET_OK);

  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 3);

  EXPECT_EQ(hazcat_unregister_subscription(cuda_sub), RMW_RET_OK);

  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 2);

  EXPECT_EQ(hazcat_unregister_subscription(cpu_sub2), RMW_RET_OK);

  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 1);

  cuda_ringbuf_unmap(
    reinterpret_cast<hma_allocator_t *>(
      reinterpret_cast<pub_sub_data_t *>(cuda_pub->data)->alloc));
  rmw_free(cuda_pub->data);
  rmw_free(cuda_sub->data);
  rmw_free(cpu_sub2->data);
  rmw_publisher_free(cuda_pub);
  rmw_subscription_free(cuda_sub);
  rmw_subscription_free(cpu_sub2);
}

TEST_F(MessageQueueTest, unregister_and_destroy) {
  message_queue_t * mq = mq_node->elem;

  EXPECT_EQ(hazcat_unregister_publisher(cpu_pub), RMW_RET_OK);

  EXPECT_EQ(mq->pub_count, 0);
  EXPECT_EQ(mq->sub_count, 1);

  EXPECT_EQ(hazcat_unregister_subscription(cpu_sub), RMW_RET_OK);

  // Reopen file
  char shmem_file[128];
  snprintf(shmem_file, sizeof(shmem_file), "/ros2_hazcat.%s", cpu_pub->topic_name);
  int fd = shm_open(mq_node->file_name, O_CREAT | O_RDWR, 0777);
  ASSERT_NE(fd, -1);

  // File should be empty, confirming it was deleted before
  struct stat st;
  fstat(fd, &st);
  EXPECT_EQ(st.st_size, 0);

  // Now remove it again
  EXPECT_EQ(shm_unlink(mq_node->file_name), 0);

  EXPECT_EQ(hazcat_fini(), RMW_RET_OK);

  cpu_ringbuf_unmap(
    reinterpret_cast<hma_allocator_t *>(
      reinterpret_cast<pub_sub_data_t *>(cpu_pub->data)->alloc));
  rmw_free(cpu_pub->data);
  rmw_free(cpu_sub->data);
  rmw_publisher_free(cpu_pub);
  rmw_subscription_free(cpu_sub);
}
