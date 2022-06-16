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

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#ifdef __cplusplus
extern "C"
{
#endif
rmw_ret_t
rmw_init_publisher_allocation(
  const rosidl_message_type_support_t * type_support,
  const rosidl_runtime_c__Sequence__bound * message_bounds,
  rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_bounds, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_init_publisher_allocation hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_fini_publisher_allocation(rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_fini_publisher_allocation hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_publisher_t *
rmw_create_publisher(
  const rmw_node_t * node,
  const rosidl_message_type_support_t * type_supports,
  const char * topic_name,
  const rmw_qos_profile_t * qos_policies,
  const rmw_publisher_options_t * publisher_options)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_name, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(qos_policies, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher_options, NULL);

  RMW_SET_ERROR_MSG("rmw_create_publisher hasn't been implemented yet");
  return NULL;
}

rmw_ret_t
rmw_destroy_publisher(rmw_node_t * node, rmw_publisher_t * publisher)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_destroy_publisher hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_gid_for_publisher(const rmw_publisher_t * publisher, rmw_gid_t * gid)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(gid, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publisher_assert_liveliness hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publisher_assert_liveliness(const rmw_publisher_t * publisher)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publisher_assert_liveliness hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publisher_wait_for_all_acked(const rmw_publisher_t * publisher, rmw_time_t wait_timeout)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  (void)wait_timeout;

  RMW_SET_ERROR_MSG("rmw_publisher_wait_for_all_acked hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publisher_get_actual_qos(const rmw_publisher_t * publisher, rmw_qos_profile_t * qos)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(qos, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publisher_get_actual_qos hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publish(
  const rmw_publisher_t * publisher,
  const void * ros_message,
  rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publish hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publish_serialized_message(
  const rmw_publisher_t * publisher,
  const rmw_serialized_message_t * serialized_message,
  rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publish_serialized_message hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_borrow_loaned_message(
  const rmw_publisher_t * publisher,
  const rosidl_message_type_support_t * type_support,
  void ** ros_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_borrow_loaned_message hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_return_loaned_message_from_publisher(const rmw_publisher_t * publisher, void * loaned_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(loaned_message, RMW_RET_ERROR);
  
  RMW_SET_ERROR_MSG("rmw_return_loaned_message_from_publisher hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_publish_loaned_message(
  const rmw_publisher_t * publisher,
  void * ros_message,
  rmw_publisher_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(publisher, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_publish_loaned_message hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif