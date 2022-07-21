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

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#ifdef __cplusplus
extern "C"
{
#endif
rmw_wait_set_t *
rmw_create_wait_set(rmw_context_t * context, size_t max_conditions)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, NULL);
  (void)max_conditions;

  RMW_SET_ERROR_MSG("rmw_create_wait_set hasn't been implemented yet");
  return NULL;
}

rmw_ret_t
rmw_destroy_wait_set(rmw_wait_set_t * wait_set)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_ERROR);

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
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscriptions, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_conditions, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(services, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(clients, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(events, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_set, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(wait_timeout, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_wait hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
