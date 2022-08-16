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

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_hazcat/types.h"

#ifdef __cplusplus
extern "C"
{
#endif
rmw_wait_set_t *
rmw_create_wait_set(rmw_context_t * context, size_t max_conditions)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, NULL);

  waitset_t * ws = rmw_allocate(sizeof(waitset_t) + max_conditions * sizeof(struct epoll_event));
  // ws->num_subs = 0;
  // ws->num_gcs = 0;
  // ws->num_cls = 0;
  // ws->num_srv = 0;
  // ws->num_evt = 0;
  // ws->subscriptions = (rmw_subscription_t*)(ws+1);
  // ws->guard_conditions = (rmw_guard_condition_t*)(ws+1);
  // ws->clients = (rmw_client_t*)(ws+1);
  // ws->services = (rmw_service_t*)(ws+1);
  // ws->events = (rmw_events_t*)(ws+1);

  rmw_wait_set_t * rmw_ws = rmw_wait_set_allocate();
  rmw_ws->data = (void*)ws;
  rmw_ws->implementation_identifier = rmw_get_implementation_identifier();

  return rmw_ws;
}

rmw_ret_t
rmw_destroy_wait_set(rmw_wait_set_t * wait_set)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_INVALID_ARGUMENT);

  RMW_SET_ERROR_MSG("rmw_destroy_wait_set hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_wait(
  rmw_subscriptions_t * subscriptions,
  rmw_guard_conditions_t * guard_conditions,
  rmw_services_t * services,
  rmw_clients_t * clients,
  rmw_events_t * events,
  rmw_wait_set_t * wait_set,
  const rmw_time_t * wait_timeout)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscriptions, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_conditions, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(services, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(clients, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(events, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_timeout, RMW_RET_INVALID_ARGUMENT);

  waitset_t * ws = (waitset_t *)wait_set->data;
  ws->len = 0;

  // Each message queue associated with a topic has an rmw_guard_condition. We collect all of them
  // from each subscription's topic, add them all to a poll/epoll. Similar approach for services
  // and clients. guard_conditions are just added directly. No strategy for events. Waiting on the
  // poll/epoll will reveal which topics or guards are ready

  for(int i = 0; i < subscriptions->subscriber_count; i++) {
    #ifdef __linux__
    guard_condition_t * gc = &((pub_sub_data_t *)((rmw_subscription_t *)subscriptions->subscribers[0])->data)->mq->elem->gc;
    if (epoll_ctl(ws->epollfd, EPOLL_CTL_ADD, gc->pfd[1], &gc->ev) == -1 && errno != EEXIST) {
      RMW_SET_ERROR_MSG("Unable to wait on subscription");
      return RMW_RET_ERROR;
    }
    #else
    // TODO(nightduck): Use poll instead
    #endif
    ws->len++;
  }

  for(int i = 0; i < guard_conditions->guard_condition_count; i++) {
    #ifdef __linux__
    guard_condition_t * gc = (guard_condition_t *)((rmw_subscription_t *)guard_conditions->guard_conditions[i])->data;
    if (epoll_ctl(ws->epollfd, EPOLL_CTL_ADD, gc->pfd[1], &gc->ev) == -1) {
      RMW_SET_ERROR_MSG("Unable to wait on guard condition");
      return RMW_RET_ERROR;
    }
    #else
    // TODO(nightduck): Use poll instead
    #endif
    ws->len++;
  }

  // Calculate timeout and wait
  int timeout;
  if (wait_timeout == NULL) {
    timeout = -1;
  } else {
    timeout = wait_timeout->sec * 1000 + wait_timeout->nsec / 1000000;
  }
  #ifdef __linux__
  int ready = epoll_wait(ws->epollfd, ws->evlist, ws->len, timeout);
  if (ready == -1) {
    RMW_SET_ERROR_MSG("rmw_wait error in epoll_wait");
    return RMW_RET_ERROR;
  }
  for(int i = 0; i < ready; i++) {

  }
  #else
  // TODO(nightduck): Use poll instead
  #endif


  // // While timeout not expired and no triggers
  //   for(int i = 0; i < subscriptions->subscriber_count; i++) {
  //     // if subscription available
  //       ws->subscriptions[ws->num_subs++] = subscriptions->subscribers[i];
  //   }
  //   for(int i = 0; i < guard_conditions->guard_condition_count; i++) {
  //     // if guard condition triggered available
  //       ws->guard_conditions[ws->num_gcs++] = guard_conditions->guard_conditions[i];
  //   }



  RMW_SET_ERROR_MSG("rmw_wait hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
