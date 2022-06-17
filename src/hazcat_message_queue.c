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

#include <linux/limits.h>   // TODO: Portability

#include "hazcat_message_queue.h"j

bool
hazcat_register_publisher(rmw_publisher_t * pub, rmw_qos_profile_t * qos)
{
    char shmem_file[NAME_MAX];  // TODO: Portability
    
    // TODO: Check if message queue exists, if not, shm_open and mmap it

    message_queue_t * mq;

    if (qos->depth > mq->len) {
        // TODO: Resize operation
    }

    int i;
    for(i = 0; i < DOMAINS_PER_TOPIC; i++) {
        if (pub->data.domain == mq->domains[i]) {
            break;
        }
    }
    if (i == DOMAINS_PER_TOPIC) {
        if (mq->num_domains == DOMAINS_PER_TOPIC) {
            RMW_SET_ERROR_MSG("Publisher registration failed. Maximum number of memory domains per \
                topic exceeded");
            return RMW_RET_ERROR;
        }

        // TODO: Resize operation
    }

    // TODO: Track publisher data somehow
}


#ifdef __cplusplus
}
#endif