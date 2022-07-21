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
rmw_ret_t
rmw_count_publishers(
  const rmw_node_t * node,
  const char * topic_name,
  size_t * count)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_name, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(count, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_count_publishers hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_count_subscribers(
  const rmw_node_t * node,
  const char * topic_name,
  size_t * count)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_name, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(count, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_count_subscribers hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_subscription_count_matched_publishers(
  const rmw_subscription_t * subscription,
  size_t * publisher_count)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher_count, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_subscription_count_matched_publishers hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publisher_count_matched_subscriptions(
  const rmw_publisher_t * publisher,
  size_t * subscription_count)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription_count, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publisher_count_matched_subscriptions hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
