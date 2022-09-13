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

#ifdef __cplusplus
extern "C"
{
#endif

#define _GNU_SOURCE

#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/limits.h>  // TODO(nightduck): Portability. Offers NAME_MAX for shmem_file below

#include "rmw_hazcat/hashtable.h"
#include "rmw_hazcat/hazcat_message_queue.h"
#include "rmw_hazcat/guard_condition.h"


char fifo_file[NAME_MAX] = "/tmp/ros2_hazcat";
char shmem_file[NAME_MAX] = "/ros2_hazcat";
const int shm_file_offset = 12;
const int fifo_file_offset = 16;

mq_node_t mq_list = {NULL, NULL, -1, NULL};

// Hash table linking all known allocators for this process (will be shared among message queues)
hashtable_t * ht;

rmw_ret_t
hazcat_init()
{
  ht = hashtable_init(128);
  if (NULL == ht) {
    RMW_SET_ERROR_MSG("Couldn't initialize hazcat middleware");
    return RMW_RET_ERROR;
  } else {
    return RMW_RET_OK;
  }
}

rmw_ret_t
hazcat_fini()
{
  // Clear mq list, any existing entries are in use by other processes
  mq_node_t * it = mq_list.next;
  while (NULL != it) {
    mq_list.next = it->next;
    rmw_free(it);
    it = mq_list.next;
  }
  mq_list.next = NULL;
  mq_list.elem = NULL;
  mq_list.fd = -1;
  mq_list.file_name = NULL;

  hashtable_fini(ht);
  return RMW_RET_OK;
}

inline void lock_domain(atomic_uint_fast32_t * lock, int bit_mask)
{
  atomic_uint_fast32_t val = atomic_load(lock);
  while (!atomic_compare_exchange_weak(lock, &val, bit_mask & val)) {continue;}
}

inline ref_bits_t * get_ref_bits(message_queue_t * mq, int i)
{
  return (ref_bits_t *)((uint8_t *)mq + sizeof(message_queue_t) + i * sizeof(ref_bits_t));
}

inline entry_t * get_entry(message_queue_t * mq, int domain, int i)
{
  return (entry_t *)((uint8_t *)mq + sizeof(message_queue_t) + mq->len * sizeof(ref_bits_t) +
         domain * mq->len * sizeof(entry_t) + i * sizeof(entry_t));
}

hma_allocator_t * lookup_allocator(int shmem_id)
{
  hma_allocator_t * alloc = hashtable_get(ht, shmem_id);
  if (NULL == alloc) {
    // Doesn't exist in this process yet, map it in
    alloc = remap_shared_allocator(shmem_id);
    hashtable_insert(ht, shmem_id, alloc);
    return alloc;
  }
}

// Convenient utility method since 95% of registering subscription is same as registering publisher
rmw_ret_t
hazcat_register_pub_or_sub(pub_sub_data_t * data, const char * topic_name)
{
  // Register associated allocator, so we can lookup address given shared mem id
  hashtable_insert(ht, data->alloc->shmem_id, data->alloc);

  // Add header, and replace all slashes with periods (because no subdirs in /dev/shm)
  snprintf(shmem_file + shm_file_offset, NAME_MAX - shm_file_offset, topic_name);
  char * current_pos = shmem_file + shm_file_offset;  // Set to index of first period
  while (current_pos) {
    *current_pos = '.';
    current_pos = strchr(current_pos, '/');
  }
  snprintf(fifo_file + fifo_file_offset, NAME_MAX - fifo_file_offset, shmem_file + shm_file_offset);
  message_queue_t * mq;

  // Check message queue has been opened in this process yet. If not, do so and map it
  mq_node_t * it = mq_list.next;
  while (NULL != it && 0 != strcmp(shmem_file, it->file_name)) {
    it = it->next;
  }
  if (NULL == it) {
    // Allocate some memory for mq_list insertion below. If unable to, fail before shm operations
    char * file_name = rmw_allocate(strlen(shmem_file) + 1);
    if (NULL == file_name) {
      RMW_SET_ERROR_MSG("Failed to allocate string for filename");
      return RMW_RET_ERROR;
    }
    it = rmw_allocate(sizeof(mq_node_t));
    if (NULL == it) {
      RMW_SET_ERROR_MSG("Failed to allocate memory for mq_node_t");
      return RMW_RET_ERROR;
    }

    // Make it through the list without finding a match, so it hasn't been open here yet
    int fd = shm_open(shmem_file, O_CREAT | O_RDWR | O_ASYNC, 0600);
    if (-1 == fd) {
      RMW_SET_ERROR_MSG("Couldn't open shared message queue");
      printf("Couldn't open shared message queue, %s : %d\n", shmem_file, errno);
      return RMW_RET_ERROR;
    }

    // Create accompanying fifo to send signals with
    int fifo_fd = open(fifo_file, O_RDWR | O_ASYNC);
    if (-1 == fifo_fd && ENOENT == errno) {
      if (mkfifo(fifo_file, 0666) != 0) {
        perror("mkfifo: ");
        RMW_SET_ERROR_MSG("Couldn't create signaling fifo for topics");
        printf("Couldn't create signaling fifo for topics, %s : %d\n", fifo_file, errno);
        return RMW_RET_ERROR;
      }
      fifo_fd = open(fifo_file, O_RDWR | O_ASYNC);
    }
    if (-1 == fifo_fd) {
      perror("open: ");
      printf("Couldn't create signaling fifo for topics, %s : %d\n", fifo_file, errno);
      RMW_SET_ERROR_MSG("Couldn't open signaling fifo for topics");
      return RMW_RET_ERROR;
    }

    // Acquire lock on shared file
    struct flock fl = {F_WRLCK, SEEK_SET, 0, 0, 0};
    if (-1 == fcntl(fd, F_SETLKW, &fl)) {
      RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
      return RMW_RET_ERROR;
    }

    // Check size of file, if zero, we're the first to create it, so do some initializing
    struct stat st;
    if (fstat(it->fd, &st)) {
      perror("fstat");
      RMW_SET_ERROR_MSG("Error getting size of potentially new message queue");
      return RMW_RET_ERROR;
    }
    if (0 == st.st_size) {
      // TODO(nightduck): Use history policy more intelligently so page alignment can inform depth
      size_t mq_size = sizeof(message_queue_t) + data->depth * sizeof(ref_bits_t) +
        data->depth * sizeof(entry_t);
      if (-1 == ftruncate(fd, mq_size)) {
        RMW_SET_ERROR_MSG("Couldn't resize shared message queue during creation");
        return RMW_RET_ERROR;
      }

      mq = mmap(NULL, mq_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (MAP_FAILED == mq) {
        RMW_SET_ERROR_MSG("Failed to map shared message queue into process");
        return RMW_RET_ERROR;
      }

      mq->index = 0;
      mq->len = data->depth;
      mq->num_domains = 1;
      mq->domains[0] = CPU;  // Domain 0 should always be main memory
      if (CPU != data->alloc->domain) {
        mq->num_domains++;
        mq->domains[1] = data->alloc->domain;
      }
      mq->pub_count = 0;  // One of these will be incremented after function returns
      mq->sub_count = 0;

      // // Get guard condition and copy it completely within message queue
      // rmw_guard_condition_t * gc;
      // if ((gc = rmw_create_guard_condition(data->context)) == NULL) {
      //   return RMW_RET_ERROR;
      // }
      // // TODO(nightduck): Make method for in place creation to avoid this memory leak
      // copy_guard_condition(&mq->gc, &mq->gc_impl, gc);
    } else {
      mq = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (MAP_FAILED == mq) {
        RMW_SET_ERROR_MSG("Failed to map shared message queue into process");
        return RMW_RET_ERROR;
      }
    }

    // Insert mq into mq_list
    snprintf(file_name, NAME_MAX, shmem_file);
    it->next = mq_list.next;
    it->file_name = file_name;
    it->fd = fd;
    it->signalfd = fifo_fd;
    it->elem = mq;
    mq_list.next = it;
  } else {
    mq = it->elem;

    // Acquire lock on shared file
    struct flock fl = {F_WRLCK, SEEK_SET, 0, 0, 0};
    if (fcntl(it->fd, F_SETLKW, &fl) == -1) {
      RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
      return RMW_RET_ERROR;
    }
  }

  bool needs_resize = false;

  int i;
  for (i = 0; i < DOMAINS_PER_TOPIC; i++) {
    if (data->alloc->domain == mq->domains[i]) {
      // Let pub or sub know where to find messages of their domain in this message queue
      data->array_num = i;
      break;
    }
  }
  if (DOMAINS_PER_TOPIC == i) {  // Message queue doesn't contain preferred memory domain yet
    if (DOMAINS_PER_TOPIC == mq->num_domains) {
      char * err =
        "Publisher registration failed."
        "Maximum number of memory domains per topic exceeded";
      RMW_SET_ERROR_MSG(err);
      return RMW_RET_ERROR;
    }


    // Let pub or sub know where to find messages of their domain in this message queue
    data->array_num = mq->num_domains;

    // Make note of this new domain
    mq->domains[mq->num_domains] = data->alloc->domain;
    mq->num_domains++;
    needs_resize = true;
  }

  if (data->depth > mq->len) {
    mq->len = data->depth;
    needs_resize = true;
  }

  if (needs_resize) {
    // Store old size
    struct stat st;
    if (fstat(it->fd, &st)) {
      perror("fstat");
      RMW_SET_ERROR_MSG("Error getting size of message queue");
      return RMW_RET_ERROR;
    }

    // TODO(nightduck): Use history policy more intelligently so page alignment can reccomend depth
    // Calculate new size
    size_t mq_size = sizeof(message_queue_t) + data->depth * sizeof(ref_bits_t) +
      data->depth * mq->num_domains * sizeof(entry_t);

    // Remove old copy
    if (-1 == munmap(mq, st.st_size)) {
      RMW_SET_ERROR_MSG("Unable to unmap message queue during resize");
      return RMW_RET_ERROR;
    }

    // Resize
    if (-1 == ftruncate(it->fd, mq_size)) {
      RMW_SET_ERROR_MSG("Couldn't resize shared message queue");
      return RMW_RET_ERROR;
    }

    // Remap it
    if (fstat(it->fd, &st)) {
      perror("fstat");
      RMW_SET_ERROR_MSG("Error getting new size of message queue");
      return RMW_RET_ERROR;
    }
    mq = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, it->fd, 0);
    if (MAP_FAILED == mq) {
      RMW_SET_ERROR_MSG("Failed to map shared message queue into process");
      return RMW_RET_ERROR;
    }
  }

  // Let publisher know where to find its message queue
  data->mq = it;

  return RMW_RET_OK;
}

// TODO(nightduck): Don't need to specify qos, can extract from pub
rmw_ret_t
hazcat_register_publisher(rmw_publisher_t * pub)
{
  int ret = hazcat_register_pub_or_sub(pub->data, pub->topic_name);  // Heavy lifting here
  if (RMW_RET_OK != ret) {
    return ret;
  }

  mq_node_t * it = ((pub_sub_data_t *)pub->data)->mq;

  // TODO(nightduck): Generic macros in case I want to change the type of this thing.
  //       Eg (typeof(mq->pub_count))~(typeof(mq->pub_count))0
  if (UINT16_MAX > it->elem->pub_count) {
    it->elem->pub_count++;
  } else {
    RMW_SET_ERROR_MSG("Maximum number of publishers exceeded on shared message queue");
    return RMW_RET_ERROR;
  }

  // ((pub_sub_data_t *)pub->data)->signalfd = -1;

  // Release lock
  struct flock fl = {F_UNLCK, SEEK_SET, 0, 0, 0};
  if (-1 == fcntl(it->fd, F_SETLK, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
    return RMW_RET_ERROR;
  }

  return RMW_RET_OK;
}

rmw_ret_t
hazcat_register_subscription(rmw_subscription_t * sub)
{
  int ret = hazcat_register_pub_or_sub(sub->data, sub->topic_name);  // Heavy lifting here
  if (RMW_RET_OK != ret) {
    return ret;
  }

  mq_node_t * it = ((pub_sub_data_t *)sub->data)->mq;

  // Set next index to look at, ignore any existing messages in queue, this is in line with the
  // volatile rmw_qos_durability policy
  ((pub_sub_data_t *)sub->data)->next_index = it->elem->index;

  // TODO(nightduck): Generic macros in case I want to change the type of this thing.
  //       Eg (typeof(mq->pub_count))~(typeof(mq->pub_count))0
  if (UINT16_MAX > it->elem->sub_count) {
    it->elem->sub_count++;
  } else {
    RMW_SET_ERROR_MSG("Maximum number of publishers exceeded on shared message queue");
    ret = RMW_RET_ERROR;
  }

  // sigset_t sigmask;
  // sigemptyset(&sigmask);
  // sigaddset(&sigmask, SIGMSG);
  // ((pub_sub_data_t *)sub->data)->signalfd = signalfd(it->fd, &sigmask, SFD_NONBLOCK);
  // if (((pub_sub_data_t *)sub->data)->signalfd == -1) {
  //   perror("signalfd: ");
  //   RMW_SET_ERROR_MSG("Unable to set signalfd for subscription\n");
  //   ret = RMW_RET_ERROR;
  // }

  // Release lock
  struct flock fl = {F_UNLCK, SEEK_SET, 0, 0, 0};
  if (-1 == fcntl(it->fd, F_SETLK, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
    ret = RMW_RET_ERROR;
  }

  return ret;
}

rmw_ret_t
hazcat_publish(const rmw_publisher_t * pub, void * msg, size_t len)
{
  // Acquire lock on shared file
  struct flock fl = {F_RDLCK, SEEK_SET, 0, 0, 0};
  if (-1 == fcntl(((pub_sub_data_t *)pub->data)->mq->fd, F_SETLKW, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't acquire read-lock on shared message queue");
    return RMW_RET_ERROR;
  }

  hma_allocator_t * alloc = ((pub_sub_data_t *)pub->data)->alloc;
  message_queue_t * mq = ((pub_sub_data_t *)pub->data)->mq->elem;
  int domain_col = ((pub_sub_data_t *)pub->data)->array_num;

  // Get current value of index to publish into, then increment index for next guy
  atomic_int i = atomic_fetch_add(&(mq->index), 1);

  // Then fancy compare and swap so mq->index doesn't increment into infinity
  atomic_int v = i + 1;
  while (!atomic_compare_exchange_weak(&(mq->index), &v, v % mq->len)) {continue;}

  // Get reference bits and entry to edit
  volatile ref_bits_t * ref_bits = get_ref_bits(mq, i);
  volatile entry_t * entry = get_entry(mq, domain_col, i);

  // Lock entire row
  lock_domain(&ref_bits->lock, 0xFF);

  // Release any remaining message copies
  if (0 < ref_bits->interest_count) {
    for (int d = 0; d < DOMAINS_PER_TOPIC; d++) {
      if (ref_bits->availability & (1 << d)) {
        entry_t * entry = get_entry(mq, d, i);
        hma_allocator_t * src_alloc = lookup_allocator(entry->alloc_shmem_id);
        DEALLOCATE(src_alloc, entry->offset);
      }
    }
  }

  // Store token in appropriate array, converting message pointer to expected offset value
  entry->alloc_shmem_id = alloc->shmem_id;
  entry->offset = PTR_TO_OFFSET(alloc, msg);
  entry->len = len;

  // Update reference bits
  ref_bits->availability = 1 << domain_col;
  ref_bits->interest_count = mq->sub_count;

  // Unlock row
  ref_bits->lock = 0;

  // TODO(nightduck): Add some assertions up in here, check for null pointers and all


  // Release lock on shared file
  fl.l_type = F_UNLCK;
  if (-1 == fcntl(((pub_sub_data_t *)pub->data)->mq->fd, F_SETLK, &fl)) {
    perror("fcntl");
    RMW_SET_ERROR_MSG("Couldn't release read-lock on shared message queue");
    return RMW_RET_ERROR;
  }

  // Signal that data was published
  // Signals aren't being caught be epoll and I have no idea why
  // if (-1 == fcntl(((pub_sub_data_t *)pub->data)->mq->signalfd, F_SETSIG, SIGMSG)) {
  //   perror("fcntl");
  //   RMW_SET_ERROR_MSG("Failed to signal message availability");
  //   return RMW_RET_ERROR;
  // }
  char dummy = 'e';
  if (0 >= write(((pub_sub_data_t *)pub->data)->mq->signalfd, &dummy, 1)) {
    perror("write");
    RMW_SET_ERROR_MSG("Failed to signal message availability");
    return RMW_RET_ERROR;
  }
  return RMW_RET_OK;
}

// Fetches a message reference from shared message queue.
// TODO(nightduck): Refactor alloc and message as argument references, and return
// rmw_ret_t value
msg_ref_t
hazcat_take(const rmw_subscription_t * sub)
{
  // Acquire lock on shared file
  struct flock fl = {F_RDLCK, SEEK_SET, 0, 0, 0};
  if (-1 == fcntl(((pub_sub_data_t *)sub->data)->mq->fd, F_SETLKW, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't acquire read-lock on shared message queue");
    msg_ref_t ret;
    ret.alloc = NULL;
    ret.msg = NULL;
    return ret;
  }

  hma_allocator_t * alloc = ((pub_sub_data_t *)sub->data)->alloc;
  message_queue_t * mq = ((pub_sub_data_t *)sub->data)->mq->elem;

  // Find next relevant message (skip over stale messages if we missed them)
  int i = ((pub_sub_data_t *)sub->data)->next_index;
  int history = ((pub_sub_data_t *)sub->data)->depth;
  i = ((mq->index + mq->len - i) % mq->len > history) ?
    (mq->index + mq->len - history) % mq->len : i;

  // No message available
  if (i == mq->index) {
    msg_ref_t ret;
    ret.alloc = NULL;
    ret.msg = NULL;
    return ret;
  }

  // Get message entry
  msg_ref_t ret;
  ref_bits_t * ref_bits = get_ref_bits(mq, i);
  if ((1 << ((pub_sub_data_t *)sub->data)->array_num) & ref_bits->availability) {
    // Message in preferred domain
    entry_t * entry = get_entry(mq, ((pub_sub_data_t *)sub->data)->array_num, i);

    // Lookup src allocator with hashtable mapping shm id to mem address
    hma_allocator_t * src_alloc = lookup_allocator(entry->alloc_shmem_id);

    void * msg = GET_PTR(src_alloc, entry->offset, void);

    // Zero copy condition. Increase ref count on message and use that without copy
    SHARE(src_alloc, entry->offset);
    ret.alloc = src_alloc;
    ret.msg = msg;

    // DEBUGGING
    // dump_message_queue(mq);
  } else {
    // Find first domain with a copy of this message
    // TODO(nightduck): If an allocator can bypass CPU domain on copy, they might have a
    // preferential order of domains to copy from. Take this into consideration. For now,
    // find first available
    int d = 0;
    while ((1 << d) & ~ref_bits->availability) {
      d++;
    }
    entry_t * entry = get_entry(mq, d, i);

    // Lookup src allocator with hashtable mapping shm id to mem address
    hma_allocator_t * src_alloc = lookup_allocator(entry->alloc_shmem_id);

    void * msg = GET_PTR(src_alloc, entry->offset, void);

    // Allocate space on the destination allocator (const for compiler optimization)
    const void * here = GET_PTR(alloc, ALLOCATE(alloc, entry->len), void);
    assert(here > alloc);

    int lookup_ind = alloc->strategy * NUM_DEV_TYPES + alloc->device_type;

    if (CPU == src_alloc->domain) {
      // Copy to condition on alloc
      COPY_TO(alloc, here, msg, entry->len);
    } else if (CPU == alloc->domain) {
      // Copy from condition on src_alloc
      COPY_FROM(src_alloc, msg, here, entry->len);
    } else {
      // Copy condition
      COPY(alloc, here, src_alloc, msg, entry->len);
    }

    // Store our copy for others to use
    int len = entry->len;
    entry = get_entry(mq, ((pub_sub_data_t *)sub->data)->array_num, i);
    entry->alloc_shmem_id = alloc->shmem_id;
    entry->offset = PTR_TO_OFFSET(alloc, here);
    entry->len = len;

    // Enable this domain on the availability bitmask
    ref_bits->availability |= (1 << ((pub_sub_data_t *)sub->data)->array_num);

    ret.alloc = alloc;
    ret.msg = here;

    // DEBUGGING
    // dump_message_queue(mq);
  }

  // Message queue holds one copy of each message. If this is the last subscriber, free it
  if (--(ref_bits->interest_count) <= 0) {
    for (int d = 0; d < mq->num_domains; d++) {
      if (ref_bits->availability & (1 << d)) {
        entry_t * entry = get_entry(mq, d, i);
        hma_allocator_t * src_alloc = lookup_allocator(entry->alloc_shmem_id);
        DEALLOCATE(src_alloc, entry->offset);
      }
    }
  }

  // Update for next take
  ((pub_sub_data_t *)sub->data)->next_index = (i + 1) % mq->len;

  // Release lock on shared file
  fl.l_type = F_UNLCK;
  if (-1 == fcntl(((pub_sub_data_t *)sub->data)->mq->fd, F_SETLK, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't release read-lock on shared message queue");
    // TODO(nightduck): Return something?
  }

  return ret;
}

rmw_ret_t
hazcat_unregister_publisher(rmw_publisher_t * pub)
{
  // TODO(nightduck): What if different publishers use the same allocator?
  hashtable_remove(ht, ((pub_sub_data_t *)pub->data)->alloc->shmem_id);

  mq_node_t * it = ((pub_sub_data_t *)pub->data)->mq;
  if (NULL == it) {
    RMW_SET_ERROR_MSG("Publisher not registered");
    return RMW_RET_INVALID_ARGUMENT;
  }

  // Remove pub's reference to the message queue
  ((pub_sub_data_t *)pub->data)->mq = NULL;

  // Acquire lock on message queue
  struct flock fl = {F_WRLCK, SEEK_SET, 0, 0, 0};
  if (-1 == fcntl(it->fd, F_SETLKW, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
    return RMW_RET_ERROR;
  }

  // Decrement publisher count
  if (it->elem->pub_count > 0) {
    it->elem->pub_count--;
  } else {
    RMW_SET_ERROR_MSG("Publisher count is zero when attempting to unregister.");
    return RMW_RET_ERROR;
  }

  // TODO(nightduck): See if there's a way to downscale (or don't bother)

  // If count is zero, then destroy message queue
  if (0 == it->elem->pub_count && 0 == it->elem->sub_count) {
    // // Remove it from the list (fixes bug that occurs if topic is created again)
    // mq_node_t * front = &mq_list;
    // while(front->next != it) {
    //   front = front->next;
    // }
    // front->next = it->next;

    // destroy_guard_condition_impl(&mq_list.elem->gc_impl);

    struct stat st;
    if (fstat(it->fd, &st)) {
      perror("fstat");
      RMW_SET_ERROR_MSG("Error getting size of message queue for destruction");
      return RMW_RET_ERROR;
    }
    if (munmap(it->elem, st.st_size)) {
      RMW_SET_ERROR_MSG("Error unmapping message queue");
      return RMW_RET_ERROR;
    }
    if (shm_unlink(it->file_name)) {
      RMW_SET_ERROR_MSG("Error destroying message queue");
      return RMW_RET_ERROR;
    }
  }

  // Release lock on message queue
  fl.l_type = F_UNLCK;
  if (-1 == fcntl(it->fd, F_SETLK, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
    return RMW_RET_ERROR;
  }

  return RMW_RET_OK;
}

rmw_ret_t
hazcat_unregister_subscription(rmw_subscription_t * sub)
{
  // TODO(nightduck): What if different subscriptions use the same allocator?
  hashtable_remove(ht, ((pub_sub_data_t *)sub->data)->alloc->shmem_id);

  mq_node_t * it = ((pub_sub_data_t *)sub->data)->mq;
  if (NULL == it) {
    RMW_SET_ERROR_MSG("Publisher not registered");
    return RMW_RET_INVALID_ARGUMENT;
  }

  // Remove pub's reference to the message queue
  ((pub_sub_data_t *)sub->data)->mq = NULL;

  // Acquire lock on message queue
  struct flock fl = {F_WRLCK, SEEK_SET, 0, 0, 0};
  if (-1 == fcntl(it->fd, F_SETLKW, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
    return RMW_RET_ERROR;
  }

  // Decrement subscription count
  if (it->elem->sub_count > 0) {
    it->elem->sub_count--;
  } else {
    RMW_SET_ERROR_MSG("Subscription count is zero when attempting to unregister.");
    return RMW_RET_ERROR;
  }

  // TODO(nightduck): See if there's a way to downscale (or don't bother)

  // If count is zero, then destroy message queue
  if (0 == it->elem->pub_count && 0 == it->elem->sub_count) {
    // destroy_guard_condition_impl(&mq_list.elem->gc_impl);

    struct stat st;
    if (fstat(it->fd, &st)) {
      perror("fstat");
      RMW_SET_ERROR_MSG("Error getting size of message queue for destruction");
      return RMW_RET_ERROR;
    }
    if (munmap(it->elem, st.st_size)) {
      RMW_SET_ERROR_MSG("Error unmapping message queue");
      return RMW_RET_ERROR;
    }
    if (shm_unlink(it->file_name)) {
      RMW_SET_ERROR_MSG("Error destroying message queue");
      return RMW_RET_ERROR;
    }
  }

  // Release lock on message queue
  fl.l_type = F_UNLCK;
  if (-1 == fcntl(it->fd, F_SETLK, &fl)) {
    RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
    return RMW_RET_ERROR;
  }

  return RMW_RET_OK;
}

// TODO(nightduck): Implement read lock. Otherwise, this creates an cosmetic race condition.
hma_allocator_t *
get_matching_alloc(const rmw_subscription_t * sub, const void * msg)
{
  message_queue_t * mq = ((pub_sub_data_t *)sub->data)->mq->elem;

  // dump_message_queue(mq);

  int recent = ((pub_sub_data_t *)sub->data)->next_index;
  if (recent < ((pub_sub_data_t *)sub->data)->depth) {
    recent += mq->len;
  }
  for (int i = 1; i < ((pub_sub_data_t *)sub->data)->depth; i++) {
    int index = (recent - i) % mq->len;
    entry_t * entry = get_entry(mq, ((pub_sub_data_t *)sub->data)->array_num, index);

    hma_allocator_t * msg_alloc = lookup_allocator(entry->alloc_shmem_id);
    void * entry_msg = GET_PTR(msg_alloc, entry->offset, void);
    if (entry_msg == msg) {
      return msg_alloc;
    }
  }

  // Message doesn't match
  return NULL;
}

void
dump_message_queue(const message_queue_t * mq)
{
  printf("Index:       %d\n", mq->index);
  printf("Len:         %lu\n", mq->len);
  printf("Num domains: %lu\n", mq->num_domains);
  printf("Domains: \n");
  for (int i = 0; i < DOMAINS_PER_TOPIC; i++) {
    printf("  %02d: %x\n", i, mq->domains[i]);
  }
  printf("Pub count:   %d\n", mq->pub_count);
  printf("Sub count:   %d\n", mq->sub_count);

  printf("Message queue contents\n");
  ref_bits_t * ref_bits = (ref_bits_t *)(mq + 1);
  for (int i = 0; i < mq->len; i++) {
    printf("  %03d - interest_count: %d\n", i, ref_bits[i].interest_count);
    printf("  %03d - availability:   %d\n", i, ref_bits[i].availability);
    printf("  %03d - lock:           %lu\n", i, ref_bits[i].lock);

    entry_t * entries = (entry_t *)((uint8_t *)ref_bits + mq->len * sizeof(ref_bits_t));
    for (int j = 0; j < mq->num_domains; j++) {
      if (mq->num_domains > 1) {
        printf("    Domain %d\n", j);
      }
      printf("    shmem_id: %d\n", entries[i].alloc_shmem_id);
      printf("    offset:   %d\n", entries[i].offset);
      printf("    len:      %lu\n", entries[i].len);
      entries = (entry_t *)((uint8_t *)entries + mq->len * sizeof(entry_t));
    }
  }
  printf("\n");
}
#ifdef __cplusplus
}
#endif
