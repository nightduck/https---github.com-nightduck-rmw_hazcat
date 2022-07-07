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
#include "rmw_hazcat/hazcat_message_queue.h"

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

TEST(MessageQueueTest, hashtable_test) {

}

TEST(MessageQueueTest, creation_test) {
  rmw_node_t dummy;                                     // Content doesn't matter
  rosidl_message_type_support_t dummy_type_support;     // Content doesn't matter
  rmw_qos_profile_t pub_qos, sub_qos;
  pub_qos.depth = 5;
  sub_qos.depth = 10;

  cpu_ringbuf_allocator_t * alloc = create_cpu_ringbuf_allocator(8, 10);
  rmw_publisher_options_t pub_opts;
  pub_opts.rmw_specific_publisher_payload = (void*)alloc;
  rmw_subscription_options_t sub_opts;
  sub_opts.rmw_specific_subscription_payload = (void*)alloc;

  rmw_publisher_t * pub = rmw_publisher_allocate();
  rmw_subscription_t * sub = rmw_subscription_allocate();
  pub_sub_data_t * pub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));
  pub_sub_data_t * sub_data = (pub_sub_data_t*)rmw_allocate(sizeof(pub_sub_data_t));

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

  ASSERT_EQ(hazcat_register_publisher(pub, &pub_qos), RMW_RET_OK);

  mq_node_t * mq_node = pub_data->mq;
  message_queue_t * mq = mq_node->elem;
  ASSERT_STREQ(mq_node->file_name, "/dev/shm/ros2_hazcat/test");
  ASSERT_GT(mq_node->fd, 0);

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
  ASSERT_EQ(mq->len, 10);
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

  ASSERT_EQ(hazcat_unregister_publisher(pub), RMW_RET_OK);

  ASSERT_EQ(mq->index, 0);
  ASSERT_EQ(mq->len, 10);
  ASSERT_EQ(mq->num_domains, 1);
  ASSERT_EQ(mq->domains[0], alloc->untyped.domain);
  ASSERT_EQ(mq->pub_count, 0);
  ASSERT_EQ(mq->sub_count, 1);

  ASSERT_EQ(hazcat_unregister_subscription(sub), RMW_RET_OK);

  // Reopen file
  char shmem_file[128] = "/dev/shm/ros2_hazcat/";
  strcpy(shmem_file + 21, pub->topic_name);
  int fd = shm_open(mq_node->file_name, O_CREAT | O_RDWR, 0);
  ASSERT_NE(fd, -1);

  // File should be empty, confirming it was deleted before
  struct stat st;
  fstat(fd, &st);
  ASSERT_EQ(st.st_size, 0);

  rmw_publisher_free(pub);
  rmw_subscription_free(sub);
  rmw_free(pub_data);
  rmw_free(sub_data);
  cpu_ringbuf_unmap((hma_allocator_t*)alloc);
}

TEST(MessageQueueTest, registration_test) {
  // Register new domains, mix of pubs and subs
}

TEST(MessageQueueTest, rw_test) {
  // Call publish and take, from same domain and different domain
}

TEST(MessageQueueTest, destruction_test) {
  // Unregister all pubs and subs, verify destruction of shmfile
}