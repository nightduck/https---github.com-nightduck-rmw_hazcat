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
#include "rmw_hazcat/allocators/cuda_ringbuf_allocator.h"
#include "rmw_hazcat/hazcat_message_queue.h"
#include "rmw_hazcat/hashtable.h"

#include <gtest/gtest.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string>
#include <tuple>
#include <vector>

uint8_t deref(uint8_t * ptr)
{
  return *ptr;
}

inline ref_bits_t *get_ref_bits(message_queue_t *mq, int i)
{
  return (ref_bits_t*)((uint8_t *)mq + sizeof(message_queue_t) + i * sizeof(ref_bits_t));
}

inline entry_t *get_entry(message_queue_t *mq, int domain, int i)
{
  return (entry_t*)((uint8_t *)mq + sizeof(message_queue_t) + mq->len * sizeof(ref_bits_t) + domain * mq->len * sizeof(entry_t) + i * sizeof(entry_t));
}

class MessageQueueTest : public testing::Test {
public:
  mq_node_t * mq_node;
  rmw_publisher_t * cpu_pub;
  rmw_publisher_t * cuda_pub;
  rmw_subscription_t * cpu_sub;
  rmw_subscription_t * cuda_sub;
};

TEST(HashtableTest, hashtable_test) {
  // Creation test
  hashtable_t * ht = hashtable_init(8);
  ASSERT_EQ(ht->len, 8);
  ASSERT_EQ(ht->count, 0);
  ASSERT_EQ((uint8_t*)ht->table, (uint8_t*)ht + sizeof(hashtable_t));
  for(int i = 0; i < 8; i++) {
    ASSERT_EQ(ht->table[i].next, nullptr);
    ASSERT_EQ(ht->table[i].val, nullptr);
  }

  // Attempt retrival of non-existant element
  ASSERT_EQ(hashtable_get(ht, 42), nullptr);

  // Ordinary insertion and removal test
  hashtable_insert(ht, 42, (void*)0x42);
  ASSERT_EQ(ht->table[HASH(42,8)].val, (void*)0x42);
  ASSERT_EQ(ht->table[HASH(42,8)].key, 42);
  ASSERT_EQ(ht->table[HASH(42,8)].next, nullptr);
  ASSERT_EQ(hashtable_get(ht, 42), (void*)0x42);
  hashtable_remove(ht, 42);
  ASSERT_EQ(ht->table[HASH(42,8)].val, nullptr);
  ASSERT_EQ(ht->table[HASH(42,8)].next, nullptr);
  ASSERT_EQ(hashtable_get(ht, 42), nullptr);

  // Simple collision test
  ASSERT_EQ(HASH(0x11,8), 6);
  ASSERT_EQ(HASH(0x21,8), 6);
  ASSERT_EQ(HASH(0x31,8), 6);
  hashtable_insert(ht, 0x11, (void*)0x11);    // Insert into 2nd from last slot
  hashtable_insert(ht, 0x21, (void*)0x21);    // Collide with first and land in last slot
  hashtable_insert(ht, 0x31, (void*)0x31);    // Collide, wrap around, and land in first slot
  ASSERT_EQ(ht->table[6].val, (void*)0x11);
  ASSERT_EQ(ht->table[7].val, (void*)0x21);
  ASSERT_EQ(ht->table[0].val, (void*)0x31);
  ASSERT_EQ(ht->table[6].key, 0x11);
  ASSERT_EQ(ht->table[7].key, 0x21);
  ASSERT_EQ(ht->table[0].key, 0x31);
  ASSERT_EQ(ht->table[6].next, &(ht->table[7]));
  ASSERT_EQ(ht->table[7].next, &(ht->table[0]));
  ASSERT_EQ(ht->table[0].next, nullptr);

  // Removal test
  // TODO: (remove 0x21 from above)
  hashtable_remove(ht, 0x21);
  ASSERT_EQ(ht->table[6].val, (void*)0x11);
  ASSERT_EQ(ht->table[0].val, (void*)0x31);
  ASSERT_EQ(ht->table[6].key, 0x11);
  ASSERT_EQ(ht->table[0].key, 0x31);
  ASSERT_EQ(ht->table[6].next, &(ht->table[0]));
  ASSERT_EQ(ht->table[0].next, nullptr);


  // Collision between non matching hashes, requires relocating some entries and rewriting lists
  ASSERT_EQ(HASH(0x17, 8), 0);
  ASSERT_EQ(HASH(0x27, 8), 0);
  hashtable_insert(ht, 0x21, (void*)0x21);
  hashtable_insert(ht, 0x17, (void*)0x17);
  hashtable_insert(ht, 0x27, (void*)0x27);
  ASSERT_EQ(ht->table[0].val, (void*)0x17);
  ASSERT_EQ(ht->table[1].val, (void*)0x21);
  ASSERT_EQ(ht->table[2].val, (void*)0x31);
  ASSERT_EQ(ht->table[3].val, (void*)0x27);
  ASSERT_EQ(ht->table[6].val, (void*)0x11);
  ASSERT_EQ(ht->table[0].key, 0x17);
  ASSERT_EQ(ht->table[1].key, 0x21);
  ASSERT_EQ(ht->table[2].key, 0x31);
  ASSERT_EQ(ht->table[3].key, 0x27);
  ASSERT_EQ(ht->table[6].key, 0x11);
  ASSERT_EQ(ht->table[0].next, &(ht->table[3]));
  ASSERT_EQ(ht->table[1].next, nullptr);
  ASSERT_EQ(ht->table[2].next, &(ht->table[1]));
  ASSERT_EQ(ht->table[3].next, nullptr);
  ASSERT_EQ(ht->table[6].next, &(ht->table[2]));

  // Removal test: remove head of list
  hashtable_remove(ht, 0x17);
  ASSERT_EQ(ht->table[0].val, (void*)0x27);
  ASSERT_EQ(ht->table[1].val, (void*)0x21);
  ASSERT_EQ(ht->table[2].val, (void*)0x31);
  ASSERT_EQ(ht->table[3].val, nullptr);
  ASSERT_EQ(ht->table[6].val, (void*)0x11);
  ASSERT_EQ(ht->table[0].key, 0x27);
  ASSERT_EQ(ht->table[1].key, 0x21);
  ASSERT_EQ(ht->table[2].key, 0x31);
  ASSERT_EQ(ht->table[6].key, 0x11);
  ASSERT_EQ(ht->table[0].next, nullptr);
  ASSERT_EQ(ht->table[1].next, nullptr);
  ASSERT_EQ(ht->table[2].next, &(ht->table[1]));
  ASSERT_EQ(ht->table[3].next, nullptr);
  ASSERT_EQ(ht->table[6].next, &(ht->table[2]));

  // Overwrite test
  // Insert 0x11 again, but with new value
  hashtable_insert(ht, 0x11, (void*)0x1234);
  ASSERT_EQ(ht->table[1].val, (void*)0x21);
  ASSERT_EQ(ht->table[2].val, (void*)0x31);
  ASSERT_EQ(ht->table[6].val, (void*)0x1234);
  ASSERT_EQ(ht->table[1].key, 0x21);
  ASSERT_EQ(ht->table[2].key, 0x31);
  ASSERT_EQ(ht->table[6].key, 0x11);
  ASSERT_EQ(ht->table[1].next, nullptr);
  ASSERT_EQ(ht->table[2].next, &(ht->table[1]));
  ASSERT_EQ(ht->table[6].next, &(ht->table[2]));

  hashtable_fini(ht);
}

TEST_F(MessageQueueTest, creation_and_registration) {
  rmw_node_t dummy;                                     // Content doesn't matter
  rosidl_message_type_support_t dummy_type_support;     // Content doesn't matter
  rmw_qos_profile_t pub_qos, sub_qos;
  pub_qos.depth = 5;
  sub_qos.depth = 1;

  cpu_ringbuf_allocator_t * alloc = create_cpu_ringbuf_allocator(8, 10);
  rmw_publisher_options_t pub_opts;
  pub_opts.rmw_specific_publisher_payload = (void*)alloc;
  rmw_subscription_options_t sub_opts;
  sub_opts.rmw_specific_subscription_payload = (void*)alloc;

  rmw_publisher_t * pub = rmw_publisher_allocate();
  rmw_subscription_t * sub = rmw_subscription_allocate();
  pub_sub_data_t * pub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));
  pub_sub_data_t * sub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));
  MessageQueueTest::cpu_pub = pub;
  MessageQueueTest::cpu_sub = sub;

  // Populate data->alloc with allocator
  pub_data->alloc = (hma_allocator_t*)alloc;
  sub_data->alloc = (hma_allocator_t*)alloc;

  pub->implementation_identifier = rmw_get_implementation_identifier();
  pub->data = pub_data;
  pub->topic_name = "test";
  pub->options = pub_opts;
  pub->can_loan_messages = true;

  sub->implementation_identifier = rmw_get_implementation_identifier();
  sub->data = sub_data;
  sub->topic_name = "test";
  sub->options = sub_opts;
  sub->can_loan_messages = true;

  ASSERT_EQ(hazcat_init(), RMW_RET_OK);

  ASSERT_EQ(hazcat_register_publisher(pub, &pub_qos), RMW_RET_OK);

  mq_node_t * mq_node = pub_data->mq;
  message_queue_t * mq = mq_node->elem;
  ASSERT_STREQ(mq_node->file_name, "/ros2_hazcat.test");
  ASSERT_GT(mq_node->fd, 0);
  MessageQueueTest::mq_node = mq_node;

  ASSERT_EQ(mq->index, 0);
  ASSERT_EQ(mq->len, 5);
  ASSERT_EQ(mq->num_domains, 1);
  ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
  ASSERT_EQ(mq->pub_count, 1);
  ASSERT_EQ(mq->sub_count, 0);

  ASSERT_EQ(hazcat_register_subscription(sub, &sub_qos), RMW_RET_OK);

  ASSERT_EQ(mq, sub_data->mq->elem);    // Should use same message queue
  ASSERT_EQ(mq_node, sub_data->mq);
  ASSERT_EQ(mq->index, 0);
  ASSERT_EQ(mq->len, 5);
  ASSERT_EQ(mq->num_domains, 1);
  ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
  ASSERT_EQ(mq->pub_count, 1);
  ASSERT_EQ(mq->sub_count, 1);

  // Test pub and sub data
  ASSERT_EQ(pub_data->alloc, &alloc->untyped);    // Should reference allocator
  ASSERT_EQ(sub_data->alloc, &alloc->untyped);
  ASSERT_EQ(pub_data->next_index, 0);   // Should be waiting at front of message queue
  ASSERT_EQ(sub_data->next_index, 0);
  ASSERT_EQ(pub_data->array_num, 0);    // Should use same first domain
  ASSERT_EQ(sub_data->array_num, 0);
}

TEST_F(MessageQueueTest, basic_rw) {
  // Publish twice, read once
}

TEST_F(MessageQueueTest, multi_domain_registration) {

}

// TEST_F(MessageQueueTest, multi_domain_rw) {

// }

// TEST_F(MessageQueueTest, unregister_cuda) {
//   message_queue_t * mq = MessageQueueTest::mq_node->elem;
//   rmw_publisher_t * cuda_pub = MessageQueueTest::cuda_pub;
//   rmw_subscription_t * cuda_sub = MessageQueueTest::cuda_sub;

//   ASSERT_EQ(hazcat_unregister_publisher(cuda_pub), RMW_RET_OK);

//   ASSERT_EQ(mq->pub_count, 1);
//   ASSERT_EQ(mq->sub_count, 2);

//   ASSERT_EQ(hazcat_unregister_subscription(cuda_sub), RMW_RET_OK);

//   ASSERT_EQ(mq->pub_count, 1);
//   ASSERT_EQ(mq->sub_count, 1);

//   cuda_ringbuf_unmap((hma_allocator_t*)((pub_sub_data_t*)cuda_pub->data)->alloc);
//   rmw_free(cuda_pub->data);
//   rmw_free(cuda_sub->data);
//   rmw_publisher_free(cuda_pub);
//   rmw_subscription_free(cuda_sub);
// }

TEST_F(MessageQueueTest, unregister_and_destroy) {
  message_queue_t * mq = MessageQueueTest::mq_node->elem;
  rmw_publisher_t * cpu_pub = MessageQueueTest::cpu_pub;
  rmw_subscription_t * cpu_sub = MessageQueueTest::cpu_sub;

  ASSERT_EQ(hazcat_unregister_publisher(cpu_pub), RMW_RET_OK);

  ASSERT_EQ(mq->pub_count, 0);
  ASSERT_EQ(mq->sub_count, 1);

  ASSERT_EQ(hazcat_unregister_subscription(cpu_sub), RMW_RET_OK);

  // Reopen file
  char shmem_file[128] = "/ros2_hazcat";
  strcpy(shmem_file + 13, cpu_pub->topic_name);
  int fd = shm_open(mq_node->file_name, O_CREAT | O_RDWR, 0777);
  ASSERT_NE(fd, -1);

  // File should be empty, confirming it was deleted before
  struct stat st;
  fstat(fd, &st);
  ASSERT_EQ(st.st_size, 0);

  // Now remove it again
  ASSERT_EQ(shm_unlink(mq_node->file_name), 0);

  ASSERT_EQ(hazcat_fini(), RMW_RET_OK);

  cpu_ringbuf_unmap((hma_allocator_t*)((pub_sub_data_t*)cpu_pub->data)->alloc);
  rmw_free(cpu_pub->data);
  rmw_free(cpu_sub->data);
  rmw_publisher_free(cpu_pub);
  rmw_subscription_free(cpu_sub);
}

// TEST(MessageQueueTest, creation_and_registration_test) {
//   rmw_node_t dummy;                                     // Content doesn't matter
//   rosidl_message_type_support_t dummy_type_support;     // Content doesn't matter
//   rmw_qos_profile_t pub_qos, sub_qos;
//   pub_qos.depth = 5;
//   sub_qos.depth = 10;

//   cpu_ringbuf_allocator_t * alloc = create_cpu_ringbuf_allocator(8, 10);
//   rmw_publisher_options_t pub_opts;
//   pub_opts.rmw_specific_publisher_payload = (void*)alloc;
//   rmw_subscription_options_t sub_opts;
//   sub_opts.rmw_specific_subscription_payload = (void*)alloc;

//   rmw_publisher_t * pub = rmw_publisher_allocate();
//   rmw_subscription_t * sub = rmw_subscription_allocate();
//   pub_sub_data_t * pub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));
//   pub_sub_data_t * sub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));

//   // Populate data->alloc with allocator
//   pub_data->alloc = (hma_allocator_t*)alloc;
//   sub_data->alloc = (hma_allocator_t*)alloc;

//   pub->implementation_identifier = rmw_get_implementation_identifier();
//   pub->data = pub_data;
//   pub->topic_name = "test";
//   pub->options = pub_opts;
//   pub->can_loan_messages = true;

//   sub->implementation_identifier = rmw_get_implementation_identifier();
//   sub->data = sub_data;
//   sub->topic_name = "test";
//   sub->options = sub_opts;
//   sub->can_loan_messages = true;

//   ASSERT_EQ(hazcat_register_publisher(pub, &pub_qos), RMW_RET_OK);

//   mq_node_t * mq_node = pub_data->mq;
//   message_queue_t * mq = mq_node->elem;
//   ASSERT_STREQ(mq_node->file_name, "/ros2_hazcat.test");
//   ASSERT_GT(mq_node->fd, 0);

//   ASSERT_EQ(mq->index, 0);
//   ASSERT_EQ(mq->len, 5);
//   ASSERT_EQ(mq->num_domains, 1);
//   ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
//   ASSERT_EQ(mq->pub_count, 1);
//   ASSERT_EQ(mq->sub_count, 0);

//   ASSERT_EQ(hazcat_register_subscription(sub, &sub_qos), RMW_RET_OK);

//   ASSERT_EQ(mq, sub_data->mq->elem);    // Should use same message queue
//   ASSERT_EQ(mq_node, sub_data->mq);
//   ASSERT_EQ(mq->index, 0);
//   ASSERT_EQ(mq->len, 10);
//   ASSERT_EQ(mq->num_domains, 1);
//   ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
//   ASSERT_EQ(mq->pub_count, 1);
//   ASSERT_EQ(mq->sub_count, 1);

//   // Test pub and sub data
//   ASSERT_EQ(pub_data->alloc, &alloc->untyped);    // Should reference allocator
//   ASSERT_EQ(sub_data->alloc, &alloc->untyped);
//   ASSERT_EQ(pub_data->next_index, 0);   // Should be waiting at front of message queue
//   ASSERT_EQ(sub_data->next_index, 0);
//   ASSERT_EQ(pub_data->array_num, 0);    // Should use same first domain
//   ASSERT_EQ(sub_data->array_num, 0);

//   ASSERT_EQ(hazcat_unregister_publisher(pub), RMW_RET_OK);

//   ASSERT_EQ(mq->index, 0);
//   ASSERT_EQ(mq->len, 10);
//   ASSERT_EQ(mq->num_domains, 1);
//   ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
//   ASSERT_EQ(mq->pub_count, 0);
//   ASSERT_EQ(mq->sub_count, 1);

//   ASSERT_EQ(hazcat_unregister_subscription(sub), RMW_RET_OK);

//   // Reopen file
//   char shmem_file[128] = "/ros2_hazcat";
//   strcpy(shmem_file + 13, pub->topic_name);
//   int fd = shm_open(mq_node->file_name, O_CREAT | O_RDWR, 0777);
//   ASSERT_NE(fd, -1);

//   // File should be empty, confirming it was deleted before
//   struct stat st;
//   fstat(fd, &st);
//   ASSERT_EQ(st.st_size, 0);

//   // Now remove it again
//   ASSERT_EQ(shm_unlink(mq_node->file_name), 0);

//   ASSERT_EQ(hazcat_fini(), RMW_RET_OK);

//   rmw_publisher_free(pub);
//   rmw_subscription_free(sub);
//   rmw_free(pub_data);
//   rmw_free(sub_data);
//   cpu_ringbuf_unmap((hma_allocator_t*)alloc);
// }

// TEST(MessageQueueTest, basic_rw_test) {
//   // Call publish and take, from same domain and different domain

//   rmw_node_t dummy;                                     // Content doesn't matter
//   rosidl_message_type_support_t dummy_type_support;     // Content doesn't matter
//   rmw_qos_profile_t pub_qos, sub_qos;
//   pub_qos.depth = 5;
//   sub_qos.depth = 1;

//   cpu_ringbuf_allocator_t * alloc = create_cpu_ringbuf_allocator(8, 10);
//   rmw_publisher_options_t pub_opts;
//   pub_opts.rmw_specific_publisher_payload = (void*)alloc;
//   rmw_subscription_options_t sub_opts;
//   sub_opts.rmw_specific_subscription_payload = (void*)alloc;

//   rmw_publisher_t * pub = rmw_publisher_allocate();
//   rmw_subscription_t * sub = rmw_subscription_allocate();
//   pub_sub_data_t * pub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));
//   pub_sub_data_t * sub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));

//   // Populate data->alloc with allocator
//   pub_data->alloc = (hma_allocator_t*)alloc;
//   sub_data->alloc = (hma_allocator_t*)alloc;

//   pub->implementation_identifier = rmw_get_implementation_identifier();
//   pub->data = pub_data;
//   pub->topic_name = "test";
//   pub->options = pub_opts;
//   pub->can_loan_messages = true;

//   sub->implementation_identifier = rmw_get_implementation_identifier();
//   sub->data = sub_data;
//   sub->topic_name = "test";
//   sub->options = sub_opts;
//   sub->can_loan_messages = true;

//   ASSERT_EQ(hazcat_init(), RMW_RET_OK);

//   ASSERT_EQ(hazcat_register_publisher(pub, &pub_qos), RMW_RET_OK);

//   mq_node_t * mq_node = pub_data->mq;
//   message_queue_t * mq = mq_node->elem;
//   ASSERT_STREQ(mq_node->file_name, "/ros2_hazcat.test");
//   ASSERT_GT(mq_node->fd, 0);

//   ASSERT_EQ(mq->index, 0);
//   ASSERT_EQ(mq->len, 5);
//   ASSERT_EQ(mq->num_domains, 1);
//   ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
//   ASSERT_EQ(mq->pub_count, 1);
//   ASSERT_EQ(mq->sub_count, 0);

//   ASSERT_EQ(hazcat_register_subscription(sub, &sub_qos), RMW_RET_OK);

//   ASSERT_EQ(mq, sub_data->mq->elem);    // Should use same message queue
//   ASSERT_EQ(mq_node, sub_data->mq);
//   ASSERT_EQ(mq->index, 0);
//   ASSERT_EQ(mq->len, 10);
//   ASSERT_EQ(mq->num_domains, 1);
//   ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
//   ASSERT_EQ(mq->pub_count, 1);
//   ASSERT_EQ(mq->sub_count, 1);

//   // Test pub and sub data
//   ASSERT_EQ(pub_data->alloc, &alloc->untyped);    // Should reference allocator
//   ASSERT_EQ(sub_data->alloc, &alloc->untyped);
//   ASSERT_EQ(pub_data->next_index, 0);   // Should be waiting at front of message queue
//   ASSERT_EQ(sub_data->next_index, 0);
//   ASSERT_EQ(pub_data->array_num, 0);    // Should use same first domain
//   ASSERT_EQ(sub_data->array_num, 0);

//   // Test take, should expect empty message
//   msg_ref_t msg_ref = hazcat_take(sub);
//   ASSERT_EQ(msg_ref.alloc, nullptr);
//   ASSERT_EQ(msg_ref.msg, nullptr);

//   // Test publish, message queue should update accordingly
//   int msg_offset = ALLOCATE(alloc, 8);
//   long * msg = GET_PTR(alloc, msg_offset, long);
//   ASSERT_EQ(hazcat_publish(pub, msg), RMW_RET_OK);
//   ASSERT_EQ(mq->index, 1);
//   ref_bits_t * ref_bits = get_ref_bits(mq, 0);
//   entry_t * entry = get_entry(mq, 0, 0);
//   ASSERT_EQ(ref_bits->availability, 0x1);
//   ASSERT_EQ(ref_bits->interest_count, 1);
//   ASSERT_EQ(ref_bits->lock, 0);
//   ASSERT_EQ(entry->alloc_shmem_id, alloc->shmem_id);
//   ASSERT_EQ(entry->len, 8);
//   ASSERT_EQ(entry->offset, msg_offset);

//   // Test take, should expect message just published
//   msg_ref = hazcat_take(sub);
//   ASSERT_EQ(msg_ref.msg, msg);
//   ASSERT_EQ(msg_ref.alloc, (hma_allocator_t*)alloc);
//   ASSERT_EQ(ref_bits->availability, 0x1);
//   ASSERT_EQ(ref_bits->interest_count, 0);
//   ASSERT_EQ(ref_bits->lock, 0);
//   ASSERT_EQ(entry->alloc_shmem_id, alloc->shmem_id);
//   ASSERT_EQ(entry->len, 8);
//   ASSERT_EQ(entry->offset, msg_offset);

//   // Test take, should expect empty message
//   msg_ref = hazcat_take(sub);
//   ASSERT_EQ(msg_ref.alloc, nullptr);
//   ASSERT_EQ(msg_ref.msg, nullptr);

//   // Publish 2 messages
//   int msg2_offset = ALLOCATE(alloc, 8);
//   long * msg2 = GET_PTR(alloc, msg_offset, long);
//   int msg3_offset = ALLOCATE(alloc, 8);
//   long * msg3 = GET_PTR(alloc, msg_offset, long);
//   ASSERT_EQ(hazcat_publish(pub, msg2), RMW_RET_OK);
//   ASSERT_EQ(mq->index, 2);
//   ASSERT_EQ(hazcat_publish(pub, msg3), RMW_RET_OK);
//   ASSERT_EQ(mq->index, 3);
//   ref_bits = get_ref_bits(mq, 1);
//   entry = get_entry(mq, 0, 1);
//   ASSERT_EQ(ref_bits->availability, 0x1);
//   ASSERT_EQ(ref_bits->interest_count, 1);
//   ASSERT_EQ(ref_bits->lock, 0);
//   ASSERT_EQ(entry->alloc_shmem_id, alloc->shmem_id);
//   ASSERT_EQ(entry->len, 8);
//   ASSERT_EQ(entry->offset, msg2_offset);
//   ref_bits = get_ref_bits(mq, 2);
//   entry = get_entry(mq, 0, 2);
//   ASSERT_EQ(ref_bits->availability, 0x1);
//   ASSERT_EQ(ref_bits->interest_count, 1);
//   ASSERT_EQ(ref_bits->lock, 0);
//   ASSERT_EQ(entry->alloc_shmem_id, alloc->shmem_id);
//   ASSERT_EQ(entry->len, 8);
//   ASSERT_EQ(entry->offset, msg3_offset);

//   // Test take, should only receive most recent message
//   msg_ref = hazcat_take(sub);
//   ASSERT_EQ(msg_ref.msg, msg3);
//   ASSERT_EQ(msg_ref.alloc, (hma_allocator_t*)alloc);
//   ASSERT_EQ(ref_bits->availability, 0x1);
//   ASSERT_EQ(ref_bits->interest_count, 0);
//   ASSERT_EQ(ref_bits->lock, 0);
//   ASSERT_EQ(entry->alloc_shmem_id, alloc->shmem_id);
//   ASSERT_EQ(entry->len, 8);
//   ASSERT_EQ(entry->offset, msg3_offset);

//   // Remove publisher
//   ASSERT_EQ(hazcat_unregister_publisher(pub), RMW_RET_OK);
//   ASSERT_EQ(mq->index, 3);
//   ASSERT_EQ(mq->len, 10);
//   ASSERT_EQ(mq->num_domains, 1);
//   ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
//   ASSERT_EQ(mq->pub_count, 0);
//   ASSERT_EQ(mq->sub_count, 1);

//   ASSERT_EQ(hazcat_unregister_subscription(sub), RMW_RET_OK);

//   ASSERT_EQ(hazcat_fini(), RMW_RET_OK);

//   rmw_publisher_free(pub);
//   rmw_subscription_free(sub);
//   rmw_free(pub_data);
//   rmw_free(sub_data);
//   cpu_ringbuf_unmap((hma_allocator_t*)alloc);
// }