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
#include <signal.h>
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
  if (ws == NULL) {
    RMW_SET_ERROR_MSG("Unable to allocate memory for waitset implementation");
    return NULL;
  }
  ws->evlist = (struct epoll_event *)(ws + 1);
  ws->epollfd = epoll_create(max_conditions);
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
  if (rmw_ws == NULL) {
    RMW_SET_ERROR_MSG("Unable to allocate memory for waitset implementation");
    return NULL;
  }
  rmw_ws->data = (void *)ws;
  rmw_ws->implementation_identifier = rmw_get_implementation_identifier();

  return rmw_ws;
}

rmw_ret_t
rmw_destroy_wait_set(rmw_wait_set_t * wait_set)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_ERROR);
  if (wait_set->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  rmw_ret_t ret;
  waitset_t * ws = wait_set->data;
  close(ws->epollfd);
  rmw_free(ws);
  rmw_free(wait_set);

  return RMW_RET_OK;
}

void
set_all_null(
  rmw_subscriptions_t * subscriptions,
  rmw_guard_conditions_t * guard_conditions,
  rmw_services_t * services,
  rmw_clients_t * clients,
  rmw_events_t * events)
{
  if (NULL != subscriptions) {
    for (int i = 0; i < subscriptions->subscriber_count; i++) {
      subscriptions->subscribers[i] = NULL;
    }
  }
  if (NULL != guard_conditions) {
    for (int i = 0; i < guard_conditions->guard_condition_count; i++) {
      guard_conditions->guard_conditions[i] = NULL;
    }
  }
  if (NULL != services) {
    for (int i = 0; i < services->service_count; i++) {
      services->services[i] = NULL;
    }
  }
  if (NULL != clients) {
    for (int i = 0; i < clients->client_count; i++) {
      clients->clients[i] = NULL;
    }
  }
  if (NULL != events) {
    for (int i = 0; i < events->event_count; i++) {
      events->events[i] = NULL;
    }
  }
}

#ifdef __linux__
int
clear_epoll(
  rmw_subscriptions_t * subscriptions,
  rmw_guard_conditions_t * guard_conditions,
  rmw_services_t * services,
  rmw_clients_t * clients,
  rmw_events_t * events,
  int epollfd)
{
  if (NULL != subscriptions) {
    for (int i = 0; i < subscriptions->subscriber_count; i++) {
      RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscriptions->subscribers[0], RMW_RET_ERROR);
      pub_sub_data_t * sub = (pub_sub_data_t *)subscriptions->subscribers[0];
      struct epoll_event ev = {.events = EPOLLHUP, .data = sub};
      if (epoll_ctl(epollfd, EPOLL_CTL_DEL, sub->mq->signalfd, &ev) == -1 && errno != ENOENT) {
        RMW_SET_ERROR_MSG("Unable to remove subscription from epoll");
        perror("epoll_ctl: ");
        return -1;
      }
    }
  }

  if (NULL != guard_conditions) {
    for (int i = 0; i < guard_conditions->guard_condition_count; i++) {
      RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_conditions->guard_conditions[0], RMW_RET_ERROR);
      guard_condition_t * gc = (guard_condition_t *)guard_conditions->guard_conditions[i];
      if (epoll_ctl(epollfd, EPOLL_CTL_DEL, gc->pfd[1], &gc->ev) == -1 && errno != ENOENT) {
        RMW_SET_ERROR_MSG("Unable to remove guard condition from epoll");
        perror("epoll_ctl: ");
        return -1;
      }
    }
  }
}
#endif

// NOTE: For performance reasons, it is assumed that each call to rmw_wait within a process will
// contain the same list of entities. They are placed within a epoll instance and not removed
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
  // RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscriptions, RMW_RET_INVALID_ARGUMENT);
  // RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_conditions, RMW_RET_INVALID_ARGUMENT);
  // RCUTILS_CHECK_ARGUMENT_FOR_NULL(services, RMW_RET_INVALID_ARGUMENT);
  // RCUTILS_CHECK_ARGUMENT_FOR_NULL(clients, RMW_RET_INVALID_ARGUMENT);
  // RCUTILS_CHECK_ARGUMENT_FOR_NULL(events, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_INVALID_ARGUMENT);
  if (wait_set->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  waitset_t * ws = (waitset_t *)wait_set->data;
  ws->len = 0;

  // NOTE: Each sub stores a signalfd corresponding to the file of its topic's message queue. Each
  // guard condition is just an unamed pipe. Publishing to a topic will send a signal on the message
  // queue file, to each subscriber's signalfd. These signalfds (and the guard condition pipes) are
  // added to epoll/poll, which waits on available input

  // Each message queue associated with a topic has an rmw_guard_condition. We collect all of them
  // from each subscription's topic, add them all to a poll/epoll. Similar approach for services
  // and clients. guard_conditions are just added directly. No strategy for events. Waiting on the
  // poll/epoll will reveal which topics or guards are ready

  if (NULL != subscriptions) {
    for (int i = 0; i < subscriptions->subscriber_count; i++) {
      #ifdef __linux__
      RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscriptions->subscribers[i], RMW_RET_ERROR);
      pub_sub_data_t * sub = (pub_sub_data_t *)subscriptions->subscribers[i];
      struct epoll_event ev = {.events = EPOLLIN, .data = sub->mq->signalfd};
      if (-1 == epoll_ctl(ws->epollfd, EPOLL_CTL_ADD, sub->mq->signalfd, &ev) && EEXIST != errno) {
        perror("epoll_ctl: ");
        RMW_SET_ERROR_MSG("Unable to wait on subscription");
        return RMW_RET_ERROR;
      }
      #else
      // TODO(nightduck): Use poll instead
      #endif
      ws->len++;
    }
  }

  if (NULL != guard_conditions) {
    for (int i = 0; i < guard_conditions->guard_condition_count; i++) {
      #ifdef __linux__
      RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_conditions->guard_conditions[i], RMW_RET_ERROR);
      guard_condition_t * gc = (guard_condition_t *)guard_conditions->guard_conditions[i];
      gc->ev.data.ptr = guard_conditions->guard_conditions[i];
      if (-1 == epoll_ctl(ws->epollfd, EPOLL_CTL_ADD, gc->pfd[1], &gc->ev) && EEXIST != errno) {
        perror("epoll_ctl: ");
        RMW_SET_ERROR_MSG("Unable to wait on guard condition");
        return RMW_RET_ERROR;
      }
      #else
      // TODO(nightduck): Use poll instead
      #endif
      ws->len++;
    }
  }

  if (ws->len == 0) {
    // Nothing to wait on, just return
    return RMW_RET_TIMEOUT;
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
    perror("epoll_wait: ");
    return RMW_RET_ERROR;
  } else if (ready == 0) {
    // Uncomment if you can't make guarantees about persistence of executable-to-executor assignment
    clear_epoll(subscriptions, guard_conditions, services, clients, events, ws->epollfd);

    // Timed out, set everything to null
    set_all_null(subscriptions, guard_conditions, services, clients, events);
    return RMW_RET_TIMEOUT;
  }
  // for(int i = 0; i < ready; i++) {
  //   if((ws->evlist[i].events & EPOLLERR) ||
  //     (ws->evlist[i].events & EPOLLHUP) ||
  //     (!(ws->evlist[i].events & EPOLLIN)))
  //   {
  //     RMW_SET_ERROR_MSG("Error reading triggered guard condition");
  //     continue;
  //   }
  //   ws->evlist[i].data.fd;
  #else
  // TODO(nightduck): Use poll instead
  #endif

  // See if this can clear signal fifos
  char buffer[4096];
  for (int i = 0; i < ready; i++) {
    read(ws->evlist[i].data.fd, buffer, ws->evlist[i].events);
  }

  // We don't interpret the event list from polling, we only use it to signal SOMETHING is ready,
  // manually check everything to see if it is

  if (NULL != subscriptions) {
    for (int i = 0; i < subscriptions->subscriber_count; i++) {
      // if next index and my index equal, set pointer to null, because no message available
      pub_sub_data_t * sub = (pub_sub_data_t *)subscriptions->subscribers[i];
      message_queue_t * mq = sub->mq->elem;
      if (sub->next_index == mq->index) {
        subscriptions->subscribers[i] = NULL;
      }
    }
  }
  if (NULL != guard_conditions) {
    for (int i = 0; i < guard_conditions->guard_condition_count; i++) {
      // attempt to read from pipe, if unsuccessful, set to null
      guard_condition_t * gc = guard_conditions->guard_conditions[i];
      if (guard_condition_trigger_count(gc) <= 0) {
        guard_conditions->guard_conditions[i] = NULL;
      }
    }
  }

  // Services, clients, and events not supported
  set_all_null(NULL, NULL, services, clients, events);

  return RMW_RET_OK;
}
#ifdef __cplusplus
}
#endif
