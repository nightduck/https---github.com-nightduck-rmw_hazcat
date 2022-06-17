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

rmw_ret_t
hazcat_register_publisher(rmw_publisher_t * pub, rmw_qos_profile_t * qos)
{
    strcpy(shmem_file + 21, pub->topic_name);
    message_queue_t * mq;

    // Check message queue has been opened in this process yet. If not, do so and map it
    mq_node_t * it = mq_list.next;
    while(it != NULL && strcmp(pub->topic_name, it->topic_name) != 0) {
        it = it->next;
    }
    if (it == NULL) {
        // Make it through the list without finding a match, so it hasn't been open here yet
        strcpy(shmem_file + dir_offset, pub->topic_name);
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

            mq->len = qos->depth;
            mq->num_domains = 1;
            mq->domains[0] = ((pub_sub_data_t*)pub->data)->domain;
            mq->pub_count = 1;
            mq->sub_count = 0;
        } else {
            mq = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (mq == MAP_FAILED) {
                RMW_SET_ERROR_MSG("Failed to map shared message queue into process");
                return RMW_RET_ERROR;
            }
        }

        // Insert mq into mq_list
        char * topic_name = rmw_allocate(strlen(pub->topic_name));
        strcpy(topic_name, pub->topic_name);
        it = rmw_allocate(sizeof(mq_node_t));
        it->next = mq_list.next;
        it->topic_name = topic_name;
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
        if (((pub_sub_data_t*)pub->data)->domain == mq->domains[i]) {
            break;
        }
    }
    if (i == DOMAINS_PER_TOPIC) {
        if (mq->num_domains == DOMAINS_PER_TOPIC) {
            RMW_SET_ERROR_MSG("Publisher registration failed. Maximum number of memory domains per \
                topic exceeded");
            return RMW_RET_ERROR;
        }

        mq->domains[mq->num_domains] = ((pub_sub_data_t*)pub->data)->domain;
        mq->num_domains++;
        needs_resize = true;
    }

    if (qos->depth > mq->len) {
        mq->len = qos->depth;
        needs_resize = true;
    }

    // TODO: Generic macros in case I want to change the type of this thing.
    //       Eg (typeof(mq->pub_count))~(typeof(mq->pub_count))0
    if (mq->pub_count < UINT16_MAX) {
        mq->pub_count++;
    } else {
        RMW_SET_ERROR_MSG("Maximum number of publishers exceeded on shared message queue");
        return RMW_RET_ERROR;
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
    ((pub_sub_data_t*)pub->data)->array_num = mq->num_domains - 1;
    ((pub_sub_data_t*)pub->data)->mq = it;

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
    // TODO: This doesn't interface with message queue at all. It allocates a message from the pub's
    //       allocator, stores the pointer -> shmemid and offset mapping, and returns the pointer
}

rmw_ret_t
hazcat_publish(rmw_publisher_t * pub, void * msg) {
    // TODO: Lookup allocator and offset from pointer
    
    // TODO: Store offset in message queue
}

rmw_ret_t
hazcat_unregister_publisher(rmw_publisher_t * pub)
{
    mq_node_t * it = ((pub_sub_data_t*)pub->data)->mq;

    // TODO: Lock message queue

    // TODO: Decrement publisher count

    // TODO: See if there's a way to downscale (or don't bother)
    // TODO: If count is zero, then destroy message queue

    // TODO: Release lock on message queue

    return RMW_RET_OK;
}


#ifdef __cplusplus
}
#endif