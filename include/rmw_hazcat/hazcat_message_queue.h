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

#ifndef RMW_HAZCAT__HAZCAT_MESSAGE_QUEUE_H_
#define RMW_HAZCAT__HAZCAT_MESSAGE_QUEUE_H_

#ifdef __cplusplus
# include <atomic>
# define _Atomic(X) std::atomic < X >
using std::atomic;
using std::atomic_int;
using std::atomic_uint_fast32_t;
extern "C"
{
#else
# include <stdatomic.h>
#endif

#include <stdint.h>
#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_hazcat/types.h"

// Misc initialization stuff.
rmw_ret_t
hazcat_init();

// Misc destruction stuff
rmw_ret_t
hazcat_fini();

// Registers a publisher with the zero copy buffer associated with it's name. If none exists, one
// is created. If an existing one does not accommodate the memory domain or history requirements of
// the publisher, it will be resized. Messages will not be able to be published or taken while
// this resize operation is taking place
rmw_ret_t
hazcat_register_publisher(rmw_publisher_t * pub);

// Registers a subscription with the zero copy buffer associated with it's name. If none exists, one
// is created. If an existing one does not accommodate the memory domain or history requirements of
// the subscription, it will be resized. Messages will not be able to be published or taken while
// this resize operation is taking place
rmw_ret_t
hazcat_register_subscription(rmw_subscription_t * sub);

// Stores allocator reference and message offset into message queue, has write lock on row
rmw_ret_t
hazcat_publish(const rmw_publisher_t * pub, void * msg, size_t len);

// Take's loaned message for subscriber, copying it into the correct memory domain, if needed.
// Will respect history QoS settings and skip over stale messages
msg_ref_t
hazcat_take(const rmw_subscription_t * sub);

rmw_ret_t
hazcat_unregister_publisher(rmw_publisher_t * pub);

rmw_ret_t
hazcat_unregister_subscription(rmw_subscription_t * sub);

hma_allocator_t *
get_matching_alloc(const rmw_subscription_t * sub, const void * msg);

// Used for debugging
void
dump_message_queue(const message_queue_t * mq);

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__HAZCAT_MESSAGE_QUEUE_H_
