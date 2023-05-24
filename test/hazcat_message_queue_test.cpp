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

#include "rcutils/allocator.h"
#include "rcutils/macros.h"
#include "rcutils/strdup.h"

#include "test_msgs/msg/basic_types.h"
#include "test_msgs/msg/bounded_sequences.hpp"
#include "std_msgs/msg/int32.hpp"

#include "hazcat_allocators/cpu_ringbuf_allocator.h"
#include "hazcat_allocators/cuda_ringbuf_allocator.h"
#include "hazcat/hazcat_message_queue.h"
#include "hazcat/hashtable.h"

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
  rmw_qos_profile_t pub_qos = rmw_qos_profile_system_default,
    sub_qos = rmw_qos_profile_system_default;
  rmw_init_options_t init_options;
  rmw_context_t context;
  rmw_node_t * node;
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

// Not a test, just resets and deletes zombie files from previously crashed tests
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

  init_options = rmw_get_zero_initialized_init_options();
  ASSERT_EQ(RMW_RET_OK, rmw_init_options_init(&init_options, rcutils_get_default_allocator())) <<
    rcutils_get_error_string().str;
  init_options.enclave = rcutils_strdup("/", rcutils_get_default_allocator());
  ASSERT_STREQ("/", init_options.enclave);
  context = rmw_get_zero_initialized_context();
  ASSERT_EQ(RMW_RET_OK, rmw_init(&init_options, &context)) << rcutils_get_error_string().str;
  constexpr char node_name[] = "my_test_node";
  constexpr char node_namespace[] = "/my_test_ns";
  node = rmw_create_node(&context, node_name, node_namespace, 1, true);
  ASSERT_NE(nullptr, node) << rcutils_get_error_string().str;
}

TEST_F(MessageQueueTest, creation_and_registration) {
  const rosidl_message_type_support_t * type_support =
    ROSIDL_GET_MSG_TYPE_SUPPORT(test_msgs, msg, BasicTypes);

  rmw_ret_t ret;
  size_t msg_size;
  rosidl_runtime_c__Sequence__bound dummy;
  ASSERT_EQ(RMW_RET_OK, rmw_get_serialized_message_size(type_support, &dummy, &msg_size));
  cpu_alloc = create_cpu_ringbuf_allocator(msg_size, 10);
  rmw_publisher_options_t cpu_pub_opts = {.rmw_specific_publisher_payload = cpu_alloc};
  rmw_subscription_options_t cpu_sub_opts = {.rmw_specific_subscription_payload = cpu_alloc,
    .ignore_local_publications = false};

  pub_qos.depth = 5;
  sub_qos.depth = 1;

  cpu_pub = rmw_create_publisher(node, type_support, "/test", &pub_qos, &cpu_pub_opts);
  ASSERT_NE(nullptr, cpu_pub);
  pub_sub_data_t * pub_data = reinterpret_cast<pub_sub_data_t *>(cpu_pub->data);

  struct stat st;
  fstat(pub_data->mq->fd, &st);
  EXPECT_GE(
    st.st_size,
    sizeof(message_queue_t) + pub_qos.depth * sizeof(ref_bits_t) + pub_qos.depth * sizeof(entry_t));

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

  cpu_sub = rmw_create_subscription(node, type_support, "/test", &sub_qos, &cpu_sub_opts);
  ASSERT_NE(nullptr, cpu_sub);
  pub_sub_data_t * sub_data = reinterpret_cast<pub_sub_data_t *>(cpu_sub->data);

  fstat(sub_data->mq->fd, &st);
  EXPECT_GE(
    st.st_size,
    sizeof(message_queue_t) + pub_qos.depth * sizeof(ref_bits_t) + pub_qos.depth * sizeof(entry_t));

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

  ref_bits_t * ref_bits_msg1 = get_ref_bits(mq, 0);
  ref_bits_t * ref_bits_msg2 = get_ref_bits(mq, 1);
  entry_t * entry_msg1 = get_entry(mq, 0, 0);
  entry_t * entry_msg2 = get_entry(mq, 0, 1);

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
  EXPECT_EQ(ref_bits_msg1->availability, 0x1);
  EXPECT_EQ(ref_bits_msg1->interest_count, 1);
  EXPECT_EQ(ref_bits_msg1->lock, 0);
  EXPECT_EQ(ref_bits_msg2->availability, 0x0);
  EXPECT_EQ(ref_bits_msg2->lock, 0);
  EXPECT_EQ(entry_msg1->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry_msg1->len, 8);
  EXPECT_EQ(entry_msg1->offset, msg1_offset);
  ASSERT_EQ(hazcat_publish(cpu_pub, msg2, 8), RMW_RET_OK);
  EXPECT_EQ(mq->index, 2);
  EXPECT_EQ(ref_bits_msg1->availability, 0x1);
  EXPECT_EQ(ref_bits_msg1->interest_count, 1);
  EXPECT_EQ(ref_bits_msg1->lock, 0);
  EXPECT_EQ(ref_bits_msg2->availability, 0x1);
  EXPECT_EQ(ref_bits_msg2->interest_count, 1);
  EXPECT_EQ(ref_bits_msg2->lock, 0);
  EXPECT_EQ(entry_msg2->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry_msg2->len, 8);
  EXPECT_EQ(entry_msg2->offset, msg2_offset);

  // Test take, should only receive most recent message
  msg_ref = hazcat_take(cpu_sub);
  EXPECT_EQ(msg_ref.msg, msg2);
  EXPECT_EQ(msg_ref.alloc, reinterpret_cast<hma_allocator_t *>(cpu_alloc));
  EXPECT_EQ(ref_bits_msg2->availability, 0x0);
  EXPECT_EQ(ref_bits_msg2->interest_count, 0);
  EXPECT_EQ(ref_bits_msg2->lock, 0);
  EXPECT_EQ(entry_msg2->alloc_shmem_id, cpu_alloc->shmem_id);
  EXPECT_EQ(entry_msg2->len, 8);
  EXPECT_EQ(entry_msg2->offset, msg2_offset);

  // Test take, should expect empty message
  msg_ref = hazcat_take(cpu_sub);
  EXPECT_EQ(msg_ref.alloc, nullptr);
  EXPECT_EQ(msg_ref.msg, nullptr);
}

TEST_F(MessageQueueTest, multi_domain_registration) {
  const rosidl_message_type_support_t * type_support =
    ROSIDL_GET_MSG_TYPE_SUPPORT(test_msgs, msg, BasicTypes);

  rmw_ret_t ret;
  size_t msg_size;
  rosidl_runtime_c__Sequence__bound dummy;
  ASSERT_EQ(RMW_RET_OK, rmw_get_serialized_message_size(type_support, &dummy, &msg_size));
  cuda_alloc = create_cuda_ringbuf_allocator(msg_size, 10);
  rmw_subscription_options_t cpu_sub_opts = {.rmw_specific_subscription_payload = cpu_alloc,
    .ignore_local_publications = false};
  rmw_subscription_options_t cuda_sub_opts = {.rmw_specific_subscription_payload = cuda_alloc,
    .ignore_local_publications = false};
  rmw_publisher_options_t cuda_pub_opts = {.rmw_specific_publisher_payload = cuda_alloc};

  message_queue_t * mq = mq_node->elem;

  sub_qos.depth = 10;
  cuda_sub = rmw_create_subscription(node, type_support, "/test", &sub_qos, &cuda_sub_opts);
  ASSERT_NE(nullptr, cuda_sub);
  pub_sub_data_t * cuda_sub_data = reinterpret_cast<pub_sub_data_t *>(cuda_sub->data);

  struct stat st;
  fstat(cuda_sub_data->mq->fd, &st);
  EXPECT_GE(
    st.st_size,
    sizeof(message_queue_t) + sub_qos.depth * sizeof(ref_bits_t) +
    2 * sub_qos.depth * sizeof(entry_t));

  EXPECT_EQ(mq_node, cuda_sub_data->mq);
  EXPECT_EQ(mq, cuda_sub_data->mq->elem);
  EXPECT_EQ(mq->index, 2);
  EXPECT_EQ(mq->len, 10);
  EXPECT_EQ(mq->num_domains, 2);
  EXPECT_EQ(mq->domains[1], cuda_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 2);

  pub_qos.depth = 5;
  cuda_pub = rmw_create_publisher(node, type_support, "/test", &pub_qos, &cuda_pub_opts);
  ASSERT_NE(nullptr, cuda_pub);
  pub_sub_data_t * pub_data = reinterpret_cast<pub_sub_data_t *>(cuda_pub->data);

  fstat(pub_data->mq->fd, &st);
  EXPECT_GE(
    st.st_size,
    sizeof(message_queue_t) + sub_qos.depth * sizeof(ref_bits_t) +
    2 * sub_qos.depth * sizeof(entry_t));

  EXPECT_EQ(mq_node, pub_data->mq);
  EXPECT_EQ(mq, pub_data->mq->elem);
  EXPECT_EQ(mq->index, 2);
  EXPECT_EQ(mq->len, 10);
  EXPECT_EQ(mq->num_domains, 2);
  EXPECT_EQ(mq->domains[1], cuda_alloc->untyped.domain);
  EXPECT_EQ(mq->pub_count, 2);
  EXPECT_EQ(mq->sub_count, 2);

  sub_qos.depth = 10;
  cpu_sub2 = rmw_create_subscription(node, type_support, "/test", &sub_qos, &cpu_sub_opts);
  ASSERT_NE(nullptr, cpu_sub2);
  pub_sub_data_t * cpu_sub_data = reinterpret_cast<pub_sub_data_t *>(cpu_sub2->data);

  fstat(pub_data->mq->fd, &st);
  EXPECT_GE(
    st.st_size,
    sizeof(message_queue_t) + sub_qos.depth * sizeof(ref_bits_t) +
    2 * sub_qos.depth * sizeof(entry_t));

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
  EXPECT_EQ(ref_bits->availability, 0x3);   // NOTE: Once domain-specific deallocations are
  EXPECT_EQ(ref_bits->interest_count, 1);   // implemented, above line may be 0x1
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
  EXPECT_EQ(ref_bits->availability, 0x0);
  EXPECT_EQ(ref_bits->interest_count, 0);
  EXPECT_EQ(ref_bits->lock, 0);
}

TEST_F(MessageQueueTest, unregister_cuda) {
  message_queue_t * mq = MessageQueueTest::mq_node->elem;
  rmw_publisher_t * cuda_pub = MessageQueueTest::cuda_pub;
  rmw_subscription_t * cuda_sub = MessageQueueTest::cuda_sub;

  hma_allocator_t * cuda_alloc = reinterpret_cast<hma_allocator_t *>(
    reinterpret_cast<pub_sub_data_t *>(cuda_pub->data)->alloc);

  EXPECT_EQ(RMW_RET_OK, rmw_destroy_publisher(node, cuda_pub));

  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 3);

  EXPECT_EQ(RMW_RET_OK, rmw_destroy_subscription(node, cuda_sub));

  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 2);

  EXPECT_EQ(RMW_RET_OK, rmw_destroy_subscription(node, cpu_sub2));

  EXPECT_EQ(mq->pub_count, 1);
  EXPECT_EQ(mq->sub_count, 1);

  cuda_ringbuf_unmap(cuda_alloc);
}

TEST_F(MessageQueueTest, unregister_and_destroy) {
  message_queue_t * mq = mq_node->elem;

  hma_allocator_t * cpu_alloc = reinterpret_cast<hma_allocator_t *>(
    reinterpret_cast<pub_sub_data_t *>(cpu_pub->data)->alloc);

  EXPECT_EQ(RMW_RET_OK, rmw_destroy_publisher(node, cpu_pub));

  EXPECT_EQ(mq->pub_count, 0);
  EXPECT_EQ(mq->sub_count, 1);

  EXPECT_EQ(RMW_RET_OK, rmw_destroy_subscription(node, cpu_sub));

  // Reopen file
  int fd = shm_open(mq_node->file_name, O_CREAT | O_RDWR, 0777);
  ASSERT_NE(fd, -1);

  // File should be empty, confirming it was deleted before
  struct stat st;
  fstat(fd, &st);
  EXPECT_EQ(st.st_size, 0);

  // Now remove it again
  EXPECT_EQ(shm_unlink(mq_node->file_name), 0);

  cpu_ringbuf_unmap(cpu_alloc);

  EXPECT_EQ(rmw_shutdown(&context), RMW_RET_OK);
}
