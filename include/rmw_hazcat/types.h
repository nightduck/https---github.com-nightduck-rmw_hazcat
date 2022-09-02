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

#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <signal.h>

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_hazcat/allocators/hma_template.h"

#ifndef RMW_HAZCAT__TYPES_H_
#define RMW_HAZCAT__TYPES_H_

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


#define DOMAINS_PER_TOPIC 32  // NOTE: Changes require editting ref_bits_t and lock_domain too

#define GC_FD_READ        0
#define GC_FD_WRITE       1

#define SIGMSG            SIGUSR1

typedef struct waitset
{
  // TODO(nightduck): alternate structure for non-Linux systems that don't support epoll
  size_t len;
  int epollfd;
  struct epoll_event * evlist;
} waitset_t;

typedef struct guard_condition
{
  // TODO(nightduck): alternate structure for non-Linux systems that don't support epoll
  int pfd[2];
  struct epoll_event ev;
} guard_condition_t;

typedef struct reference_bits
{
  // Indicates how many subscribers haven't read message yet. 0 indicates entry empty
  uint32_t interest_count;

  // Bitmask to indicate which domains have a copy of this message
  uint32_t availability;

  // TODO(nightduck): Replace domain locks with posix locks, specifying range of file?
  // Lock for each domain
  atomic_uint_fast32_t lock;
} ref_bits_t;

typedef struct entry
{
  int alloc_shmem_id;
  uint32_t offset;
  size_t len;
} entry_t;

typedef struct message_queue
{
  atomic_int index;   // Next free entry to publish into
  size_t len;
  size_t num_domains;

  // Domain IDs (device_type and device_number from allocator) for with each column in queue
  uint32_t domains[DOMAINS_PER_TOPIC];

  // Track interested pubs and subs
  uint16_t pub_count;
  uint16_t sub_count;

  // TODO(nightduck): Track sub count for each domain, to do staggered dealloc operations

  // After structure is an array of ref_bits_t objects numbering len
  // Following that there is an array of entry_t objects, one for each domain, each of size len
} message_queue_t;

// Little wrapper to store these references in linked list
typedef struct message_queue_node
{
  struct message_queue_node * next;
  const char * file_name;
  int fd;                   // fd of the actual topic file
  int signalfd;             // fd of an empty FIFO used to generate signals for message availability
  struct message_queue * elem;
} mq_node_t;

// Stores reference to message and it's owning parent
typedef struct message_reference
{
  hma_allocator_t * alloc;
  void * msg;
} msg_ref_t;

typedef struct sub_options
{
  int qos_history;
} sub_opts_t;

typedef struct pub_sub_data
{
  hma_allocator_t * alloc;  // Pointer to allocator used when copying to preferred domain
  mq_node_t * mq;           // Pointer to message queue associated with topic
  uint16_t next_index;      // Next index in message queue to read
  uint8_t array_num;        // Identifies domain to message queue
  int depth;                // Max backlogged messages
  size_t msg_size;          // Upperbound of message size
  rmw_gid_t gid;            // gid of publisher
  rmw_context_t * context;
  // int signalfd;          // fd of an empty FIFO used to generate signals for message availability
} pub_sub_data_t;

typedef struct srv_clt_data
{
  int dummy;
} srv_clt_data_t;

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__TYPES_H_
