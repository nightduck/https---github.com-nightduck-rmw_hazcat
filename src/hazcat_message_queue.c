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

#ifdef __cplusplus
extern "C"
{
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/limits.h>   // TODO: Portability. Offers NAME_MAX for shmem_file below

#include "rmw_hazcat/hazcat_message_queue.h"

char shmem_file[NAME_MAX] = "/dev/shm/ros2_hazcat/";
const int dir_offset = 21;

mq_node_t mq_list = {NULL, NULL, -1, NULL};

// TODO: Hash table of mapped allocators

// Convenient utility method since 95% of registering subscription is same as registering publisher
rmw_ret_t
hazcat_register_pub_or_sub(pub_sub_data_t * data, const char * topic_name, rmw_qos_profile_t * qos)
{
    strcpy(shmem_file + 21, topic_name);
    message_queue_t * mq;

    // Check message queue has been opened in this process yet. If not, do so and map it
    mq_node_t * it = mq_list.next;
    while(it != NULL && strcmp(shmem_file, it->file_name) != 0) {
        it = it->next;
    }
    if (it == NULL) {
        // Make it through the list without finding a match, so it hasn't been open here yet
        strcpy(shmem_file + dir_offset, topic_name);
        int fd = shm_open(shmem_file, O_CREAT | O_RDWR, 0);
        if (fd == -1) {
            RMW_SET_ERROR_MSG("Couldn't open shared message queue");
            return RMW_RET_ERROR;
        }

        // Acquire lock on shared file
        struct flock fl = { F_WRLCK, SEEK_SET, 0,       0,     0 };
        if (fcntl(fd, F_SETLKW, &fl) == -1) {
            RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
            return RMW_RET_ERROR;
        }

        // Check size of file, it zero, we're the first to create it, so do some initializing
        struct stat st;
        fstat(fd, &st);
        if (st.st_size == 0) {
            // TODO: Use history policy more intelligently so page alignment can reccomend depth
            size_t mq_size = sizeof(message_queue_t) + qos->depth * sizeof(ref_bits_t)
                + qos->depth * sizeof(entry_t);
            if (ftruncate(fd, mq_size) == -1) {
                RMW_SET_ERROR_MSG("Couldn't resize shared message queue during creation");
                return RMW_RET_ERROR;
            }

            mq = mmap(NULL, mq_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mq == MAP_FAILED) {
                RMW_SET_ERROR_MSG("Failed to map shared message queue into process");
                return RMW_RET_ERROR;
            }

            mq->index = 0;
            mq->len = qos->depth;
            mq->num_domains = 1;
            mq->domains[0] = CPU;     // Domain 0 should always be main memory
            if (data->alloc->domain != CPU) {
                mq->num_domains++;
                mq->domains[1] = data->alloc->domain;
            }
            mq->pub_count = 0;  // One of these will be incremented after function returns
            mq->sub_count = 0;
        } else {
            mq = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mq == MAP_FAILED) {
                RMW_SET_ERROR_MSG("Failed to map shared message queue into process");
                return RMW_RET_ERROR;
            }
        }

        // Insert mq into mq_list
        char * file_name = rmw_allocate(strlen(shmem_file));
        strcpy(file_name, shmem_file);
        it = rmw_allocate(sizeof(mq_node_t));
        it->next = mq_list.next;
        it->file_name = file_name;
        it->fd = fd;
        it->elem = mq;
        mq_list.next = it;
    } else {
        mq = it->elem;

        // Acquire lock on shared file
        struct flock fl = { F_WRLCK, SEEK_SET, 0,       0,     0 };
        if (fcntl(it->fd, F_SETLKW, &fl) == -1) {
            RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
            return RMW_RET_ERROR;
        }
    }

    bool needs_resize = false;

    int i;
    for(i = 0; i < DOMAINS_PER_TOPIC; i++) {
        if (data->alloc->domain == mq->domains[i]) {
            break;
        }
    }
    if (i == DOMAINS_PER_TOPIC) {
        if (mq->num_domains == DOMAINS_PER_TOPIC) {
            RMW_SET_ERROR_MSG("Publisher registration failed. Maximum number of memory domains per \
                topic exceeded");
            return RMW_RET_ERROR;
        }

        mq->domains[mq->num_domains] = data->alloc->domain;
        mq->num_domains++;
        needs_resize = true;
    }

    if (qos->depth > mq->len) {
        mq->len = qos->depth;
        needs_resize = true;
    }

    if (needs_resize) {
        // TODO: Use history policy more intelligently so page alignment can reccomend depth
        size_t mq_size = sizeof(message_queue_t) + qos->depth * sizeof(ref_bits_t)
            + qos->depth * sizeof(entry_t);
        if (ftruncate(it->fd, mq_size) == -1) {
            RMW_SET_ERROR_MSG("Couldn't resize shared message queue");
            return RMW_RET_ERROR;
        }
    }

    // Let publisher know where to find its message queue
    data->array_num = mq->num_domains - 1;
    data->mq = it;

    return RMW_RET_OK;
}


rmw_ret_t
hazcat_register_publisher(rmw_publisher_t * pub, rmw_qos_profile_t * qos) {
    int ret = hazcat_register_pub_or_sub(pub->data, pub->topic_name, qos);
    if (ret != RMW_RET_OK) {
        return ret;
    }

    mq_node_t * it = ((pub_sub_data_t*)pub->data)->mq;

    // TODO: Generic macros in case I want to change the type of this thing.
    //       Eg (typeof(mq->pub_count))~(typeof(mq->pub_count))0
    if (it->elem->pub_count < UINT16_MAX) {
        it->elem->pub_count++;
    } else {
        RMW_SET_ERROR_MSG("Maximum number of publishers exceeded on shared message queue");
        return RMW_RET_ERROR;
    }

    // Release lock
    struct flock fl = { F_UNLCK, SEEK_SET, 0,       0,     0 };
    if (fcntl(it->fd, F_SETLK, &fl) == -1) {
        RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
        return RMW_RET_ERROR;
    }

    return RMW_RET_OK;
}

rmw_ret_t
hazcat_register_subscription(rmw_subscription_t * sub, rmw_qos_profile_t * qos) {
    int ret = hazcat_register_pub_or_sub(sub->data, sub->topic_name, qos);
    if (ret != RMW_RET_OK) {
        return ret;
    }

    mq_node_t * it = ((pub_sub_data_t*)sub->data)->mq;

    // TODO: Generic macros in case I want to change the type of this thing.
    //       Eg (typeof(mq->pub_count))~(typeof(mq->pub_count))0
    if (it->elem->sub_count < UINT16_MAX) {
        it->elem->sub_count++;
    } else {
        RMW_SET_ERROR_MSG("Maximum number of publishers exceeded on shared message queue");
        return RMW_RET_ERROR;
    }

    // Release lock
    struct flock fl = { F_UNLCK, SEEK_SET, 0,       0,     0 };
    if (fcntl(it->fd, F_SETLK, &fl) == -1) {
        RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
        return RMW_RET_ERROR;
    }

    return RMW_RET_OK;
}

void *
hazcat_borrow(rmw_publisher_t * pub, size_t len) {
    // This doesn't interface with message queue at all. It allocates a message from the pub's
    // allocator, computes a pointer, and returns that
    hma_allocator_t * alloc = ((pub_sub_data_t*)pub->data)->alloc;
    int offset = ALLOCATE(alloc, len);
    return OFFSET_TO_PTR(alloc, offset);
}

rmw_ret_t
hazcat_publish(rmw_publisher_t * pub, void * msg) {
    hma_allocator_t * alloc = ((pub_sub_data_t*)pub->data)->alloc;
    message_queue_t * mq = ((pub_sub_data_t*)pub->data)->mq;
    
    // Increment index atomically and use result as our entry
    // TODO: Fancy compare and swap so mq->index doesn't increment into infinity and wrap around
    int i = mq->index++;
    i %= mq->len;

    // Get reference bits and entry to edit
    ref_bits_t * ref_bits = get_ref_bits(mq, i);
    entry_t * entry = get_entry(mq, ((pub_sub_data_t*)pub->data)->array_num, i);

    // Lock entire row
    lock_domain(&ref_bits->lock, 0xFF);

    // Store token in appropriate array, converting message pointer to expected offset value
    entry->alloc_shmem_id = alloc->shmem_id;
    entry->offset = PTR_TO_OFFSET(alloc, msg);

    // Unlock row
    ref_bits->lock = 0;

    // TODO: Add some assertions up in here, check for null pointers and all

    return RMW_RET_OK;
}

rmw_ret_t
hazcat_unregister_publisher(rmw_publisher_t * pub)
{
    mq_node_t * it = ((pub_sub_data_t*)pub->data)->mq;
    if (it == NULL) {
        RMW_SET_ERROR_MSG("Publisher not registered");
        return RMW_RET_INVALID_ARGUMENT;
    }

    // Remove pub's reference to the message queue
    ((pub_sub_data_t*)pub->data)->mq = NULL;

    // Acquire lock on message queue
    struct flock fl = { F_WRLCK, SEEK_SET, 0,       0,     0 };
    if (fcntl(it->fd, F_SETLKW, &fl) == -1) {
        RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
        return RMW_RET_ERROR;
    }

    // Decrement publisher count
    if (it->elem->pub_count > 0) {
        it->elem->pub_count--;
    } else {
        RMW_SET_ERROR_MSG("Publisher count is zero when attempting to unregister. \
            Unclear how this happened");
        return RMW_RET_ERROR;
    }

    // TODO: See if there's a way to downscale (or don't bother)

    // If count is zero, then destroy message queue
    if (it->elem->pub_count == 0 && it->elem->sub_count == 0) {
        struct stat st;
        fstat(it->fd, &st);
        if(munmap(it->elem, st.st_size)) {
            RMW_SET_ERROR_MSG("Error unmapping message queue");
            return RMW_RET_ERROR;
        }
        if(shm_unlink(it->file_name)) {
            RMW_SET_ERROR_MSG("Error destroying message queue");
            return RMW_RET_ERROR;
        }
    }

    // Release lock on message queue
    fl.l_type = F_UNLCK;
    if (fcntl(it->fd, F_SETLK, &fl) == -1) {
        RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
        return RMW_RET_ERROR;
    }

    return RMW_RET_OK;
}

rmw_ret_t
hazcat_unregister_subscription(rmw_subscription_t * sub)
{
    mq_node_t * it = ((pub_sub_data_t*)sub->data)->mq;
    if (it == NULL) {
        RMW_SET_ERROR_MSG("Publisher not registered");
        return RMW_RET_INVALID_ARGUMENT;
    }

    // Remove pub's reference to the message queue
    ((pub_sub_data_t*)sub->data)->mq = NULL;

    // Acquire lock on message queue
    struct flock fl = { F_WRLCK, SEEK_SET, 0,       0,     0 };
    if (fcntl(it->fd, F_SETLKW, &fl) == -1) {
        RMW_SET_ERROR_MSG("Couldn't acquire lock on shared message queue");
        return RMW_RET_ERROR;
    }

    // Decrement publisher count
    if (it->elem->sub_count > 0) {
        it->elem->sub_count--;
    } else {
        RMW_SET_ERROR_MSG("Subscription count is zero when attempting to unregister. \
            Unclear how this happened");
        return RMW_RET_ERROR;
    }

    // TODO: See if there's a way to downscale (or don't bother)

    // If count is zero, then destroy message queue
    if (it->elem->pub_count == 0 && it->elem->sub_count == 0) {
        struct stat st;
        fstat(it->fd, &st);
        if(munmap(it->elem, st.st_size)) {
            RMW_SET_ERROR_MSG("Error unmapping message queue");
            return RMW_RET_ERROR;
        }
        if(shm_unlink(it->file_name)) {
            RMW_SET_ERROR_MSG("Error destroying message queue");
            return RMW_RET_ERROR;
        }
    }

    // Release lock on message queue
    fl.l_type = F_UNLCK;
    if (fcntl(it->fd, F_SETLK, &fl) == -1) {
        RMW_SET_ERROR_MSG("Couldn't release lock on shared message queue");
        return RMW_RET_ERROR;
    }

    return RMW_RET_OK;
}


#ifdef __cplusplus
}
#endif