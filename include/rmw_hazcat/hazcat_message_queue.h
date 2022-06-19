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


#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdatomic.h>
#include <stdint.h>
#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#define DOMAINS_PER_TOPIC   16

typedef struct reference_bits {
    // Indicates how many subscribers haven't read message yet. 0 indicates entry empty
    uint32_t interest_count;

    // Bitmask to indicate which domains have a copy of this message
    uint16_t availability;

    // Lock for each domain
    atomic_uint_fast16_t lock;
} ref_bits_t;

typedef struct entry {
    int alloc_shmem_id;
    int offset;
} entry_t;

typedef struct message_queue {
    size_t len;
    size_t num_domains;

    // Domain IDs (device_type and device_number from allocator) for with each column in queue
    uint32_t domains[DOMAINS_PER_TOPIC];

    // Track interested pubs and subs
    uint16_t pub_count;
    uint16_t sub_count;

    // After structure is an array of ref_bits_t objects numbering len
    // Following that there is an array of entry_t objects, one for each domain, each of size len
} message_queue_t;

// Little wrapper to store these references in linked list
typedef struct message_queue_node {
    struct message_queue_node * next;
    const char * file_name;
    int fd;
    message_queue_t * elem;
} mq_node_t;

typedef struct pub_sub_data {
    uint32_t domain;
    uint8_t array_num;
    mq_node_t * mq;  
} pub_sub_data_t;

void lock_domain(atomic_uint_fast16_t * lock, int bit_mask) {
    atomic_uint_fast16_t val = *lock;
    while(!atomic_compare_exchange_weak(lock, &val, bit_mask & val));
}

rmw_ret_t
hazcat_register_publisher(rmw_publisher_t * pub, rmw_qos_profile_t * qos);

rmw_ret_t
hazcat_register_subscription(rmw_subscription_t * sub, rmw_qos_profile_t * qos);

void *
hazcat_borrow(rmw_publisher_t * pub, size_t len);

rmw_ret_t
hazcat_publish(rmw_publisher_t * pub, void * msg);

void *
hazcat_take(rmw_subscription_t * sub);

rmw_ret_t
hazcat_return(rmw_subscription_t * sub, void * msg);

rmw_ret_t
hazcat_unregister_publisher(rmw_publisher_t * pub);

rmw_ret_t
hazcat_unregister_subscription(rmw_subscription_t * sub);

#ifdef __cplusplus
}
#endif

#endif