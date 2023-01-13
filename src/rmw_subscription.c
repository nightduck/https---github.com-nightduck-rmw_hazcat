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
#include "rmw/event.h"
#include "rmw/rmw.h"
#include "rmw/get_node_info_and_types.h"
#include "rmw/get_service_names_and_types.h"
#include "rmw/get_topic_endpoint_info.h"
#include "rmw/get_topic_names_and_types.h"
#include "rmw/names_and_types.h"
#include "rmw/sanity_checks.h"
#include "rmw/validate_namespace.h"
#include "rmw/validate_node_name.h"

#include "hazcat_allocators/cpu_ringbuf_allocator.h"
#include "hazcat/hazcat_message_queue.h"

#ifdef __cplusplus
extern "C"
{
#endif
rmw_ret_t
rmw_init_subscription_allocation(
  const rosidl_message_type_support_t * type_supports,
  const rosidl_runtime_c__Sequence__bound * message_bounds,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_bounds, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_INVALID_ARGUMENT);

  RMW_SET_ERROR_MSG("rmw_init_subscription_allocation hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_fini_subscription_allocation(rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocation, RMW_RET_INVALID_ARGUMENT);

  RMW_SET_ERROR_MSG("rmw_fini_subscription_allocation hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_subscription_t *
rmw_create_subscription(
  const rmw_node_t * node,
  const rosidl_message_type_support_t * type_supports,
  const char * topic_name,
  const rmw_qos_profile_t * qos_policies,
  const rmw_subscription_options_t * subscription_options)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_name, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(qos_policies, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription_options, NULL);
  if (node->implementation_identifier != rmw_get_implementation_identifier()) {
    return NULL;
  }
  if (!qos_policies->avoid_ros_namespace_conventions) {
    int validation_result = RMW_TOPIC_VALID;
    rmw_ret_t ret = rmw_validate_full_topic_name(topic_name, &validation_result, NULL);
    if (RMW_RET_OK != ret) {
      return NULL;
    }
    if (RMW_TOPIC_VALID != validation_result) {
      const char * reason = rmw_full_topic_name_validation_result_string(validation_result);
      RMW_SET_ERROR_MSG_WITH_FORMAT_STRING("invalid topic name: %s", reason);
      return NULL;
    }
  }
  if (RMW_QOS_POLICY_HISTORY_UNKNOWN == qos_policies->history) {
    RMW_SET_ERROR_MSG("Invalid QoS policy");
    return NULL;
  }

  rmw_ret_t ret;
  size_t msg_size;
  rosidl_runtime_c__Sequence__bound dummy;
  if (RMW_RET_OK != (ret = rmw_get_serialized_message_size(type_supports, &dummy, &msg_size))) {
    RMW_SET_ERROR_MSG("Unable to get serialized message size");
    return NULL;
  }

  rmw_subscription_t * sub = rmw_subscription_allocate();
  if (NULL == sub) {
    RMW_SET_ERROR_MSG("Unable to allocate memory for subscription");
    return NULL;
  }
  pub_sub_data_t * data = rmw_allocate(sizeof(pub_sub_data_t));
  if (NULL == data) {
    RMW_SET_ERROR_MSG("Unable to allocate memory for subscription info");
    return NULL;
  }

  // Populate data->alloc with allocator specified and data->history with qos setting
  data->alloc = (hma_allocator_t *)subscription_options->rmw_specific_subscription_payload;
  if (NULL == data->alloc) {
    // TODO(nightduck): Remove when TLSF allocator is done
    data->alloc = create_cpu_ringbuf_allocator(msg_size, qos_policies->depth);
    if (NULL == data->alloc) {
      RMW_SET_ERROR_MSG("Unable to create allocator for subscription");
      return NULL;
    }
  }
  data->depth = qos_policies->depth;
  data->msg_size = msg_size;
  data->context = node->context;
  sem_init(&data->lock, 0, 1);

  sub->implementation_identifier = rmw_get_implementation_identifier();
  sub->data = data;
  sub->topic_name = rmw_allocate(strlen(topic_name) + 1);
  sub->options = *subscription_options;
  sub->can_loan_messages = true;

  if (NULL == sub->topic_name) {
    RMW_SET_ERROR_MSG("Unable to allocate string for subscription's topic name");
    return NULL;
  }
  snprintf(sub->topic_name, strlen(topic_name) + 1, topic_name);

  if (RMW_RET_OK != (ret = hazcat_register_subscription(sub))) {
    return NULL;
  }

  return sub;
}

rmw_ret_t
rmw_destroy_subscription(
  rmw_node_t * node,
  rmw_subscription_t * subscription)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  if (node->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  // Remove publisher from it's message queue
  rmw_ret_t ret = hazcat_unregister_subscription(subscription);
  if (RMW_RET_OK != ret) {
    return ret;
  }

  // Free all allocated memory associated with publisher
  rmw_free(subscription->topic_name);
  rmw_free(subscription->data);
  rmw_publisher_free(subscription);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_subscription_get_actual_qos(
  const rmw_subscription_t * subscription,
  rmw_qos_profile_t * qos)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(qos, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  qos->history = RMW_QOS_POLICY_HISTORY_KEEP_LAST;
  qos->depth = ((pub_sub_data_t *)subscription->data)->depth;
  qos->reliability = RMW_QOS_POLICY_RELIABILITY_RELIABLE;
  qos->durability = RMW_QOS_POLICY_DURABILITY_VOLATILE;
  qos->deadline.nsec = 0;
  qos->deadline.sec = 0;
  qos->lifespan.nsec = 0;
  qos->lifespan.sec = 0;
  qos->liveliness = RMW_QOS_POLICY_LIVELINESS_AUTOMATIC;
  qos->liveliness_lease_duration.nsec = 0;
  qos->liveliness_lease_duration.sec = 0;
  qos->avoid_ros_namespace_conventions = false;

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  // TODO(nightduck): Implement per-message size, in case messages are smaller than upper bound
  size_t size = ((pub_sub_data_t *)subscription->data)->msg_size;

  msg_ref_t msg_ref = hazcat_take(subscription);
  if (NULL == msg_ref.msg) {
    *taken = false;
    return RMW_RET_OK;
  } else {
    *taken = true;
  }

  memcpy(ros_message, msg_ref.msg, size);

  int offset = PTR_TO_OFFSET(msg_ref.alloc, msg_ref.msg);
  DEALLOCATE(msg_ref.alloc, offset);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_with_info(
  const rmw_subscription_t * subscription,
  void * ros_message,
  bool * taken,
  rmw_message_info_t * message_info,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_info, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  // TODO(nightduck): Populate message_info
  (void *)message_info;

  // TODO(nightduck): Implement per-message size, in case messages are smaller than upper bound
  size_t size = ((pub_sub_data_t *)subscription->data)->msg_size;

  msg_ref_t msg_ref = hazcat_take(subscription);
  if (NULL == msg_ref.msg) {
    *taken = false;
    return RMW_RET_OK;
  } else {
    *taken = true;
  }

  memcpy(ros_message, msg_ref.msg, size);

  int offset = PTR_TO_OFFSET(msg_ref.alloc, msg_ref.msg);
  DEALLOCATE(msg_ref.alloc, offset);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_serialized_message(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  RMW_SET_ERROR_MSG("rmw_take_serialized_message hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_take_serialized_message_with_info(
  const rmw_subscription_t * subscription,
  rmw_serialized_message_t * serialized_message,
  bool * taken,
  rmw_message_info_t * message_info,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_info, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  RMW_SET_ERROR_MSG("rmw_take_serialized_message_with_info hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_take_loaned_message(
  const rmw_subscription_t * subscription,
  void ** loaned_message,
  bool * taken,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(loaned_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  msg_ref_t msg_ref = hazcat_take(subscription);
  *loaned_message = msg_ref.msg;
  if (NULL == *loaned_message) {
    *taken = false;
  } else {
    *taken = true;
  }

  // TODO(nightduck): Check for errors in hazcat_take

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_loaned_message_with_info(
  const rmw_subscription_t * subscription,
  void ** loaned_message,
  bool * taken,
  rmw_message_info_t * message_info,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(loaned_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_info, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  // TODO(nightduck): Populate message_info
  (void *)message_info;

  msg_ref_t msg_ref = hazcat_take(subscription);
  *loaned_message = msg_ref.msg;
  if (NULL == *loaned_message) {
    *taken = false;
  } else {
    *taken = true;
  }

  // TODO(nightduck): Check for errors in hazcat_take

  return RMW_RET_OK;
}

rmw_ret_t
rmw_return_loaned_message_from_subscription(
  const rmw_subscription_t * subscription, void * loaned_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(loaned_message, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  // This is a work-around since this rmw discards the allocator reference after hazcat_take
  hma_allocator_t * alloc = get_matching_alloc(subscription, loaned_message);
  if (NULL == alloc) {
    RMW_SET_ERROR_MSG("Returning message that wasn't loaned");
    return RMW_RET_ERROR;
  }

  int offset = PTR_TO_OFFSET(alloc, loaned_message);
  DEALLOCATE(alloc, offset);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_take_event(const rmw_event_t * event_handle, void * event_info, bool * taken)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(event_handle, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(event_info, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);

  RMW_SET_ERROR_MSG("rmw_take_event hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_take_sequence(
  const rmw_subscription_t * subscription,
  size_t count,
  rmw_message_sequence_t * message_sequence,
  rmw_message_info_sequence_t * message_info_sequence,
  size_t * taken,
  rmw_subscription_allocation_t * allocation)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscription, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_sequence, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_info_sequence, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(taken, RMW_RET_INVALID_ARGUMENT);
  if (subscription->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  if (0u == count) {
    RMW_SET_ERROR_MSG("count cannot be 0");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (count > message_sequence->capacity) {
    RMW_SET_ERROR_MSG("Insufficient capacity in message_sequence");
    return RMW_RET_INVALID_ARGUMENT;
  }

  if (count > message_info_sequence->capacity) {
    RMW_SET_ERROR_MSG("Insufficient capacity in message_info_sequence");
    return RMW_RET_INVALID_ARGUMENT;
  }

  *taken = 0;
  bool taken_flag = false;
  rmw_ret_t ret = RMW_RET_OK;
  for (int i = 0; i < count; i++) {
    ret = rmw_take_with_info(
      subscription, message_sequence->data[*taken], &taken_flag,
      &message_info_sequence->data[*taken], allocation);
    if (RMW_RET_OK != ret) {
      break;
    }
    if (taken_flag) {
      (*taken)++;
    }
  }

  message_sequence->size = *taken;
  message_info_sequence->size = *taken;

  return ret;
}

rmw_ret_t
rmw_get_subscriptions_info_by_topic(
  const rmw_node_t * node, rcutils_allocator_t * allocator,
  const char * topic_name, bool no_mangle,
  rmw_topic_endpoint_info_array_t * subscriptions_info)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_name, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(subscriptions_info, RMW_RET_INVALID_ARGUMENT);
  if (node->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  int validation_result = RMW_NODE_NAME_VALID;
  rmw_ret_t ret = rmw_validate_node_name(topic_name, &validation_result, NULL);
  if (RMW_RET_OK != ret) {
    return ret;
  }
  if (RMW_NODE_NAME_VALID != validation_result) {
    const char * reason = rmw_node_name_validation_result_string(validation_result);
    RMW_SET_ERROR_MSG_WITH_FORMAT_STRING("node_name argument is invalid: %s", reason);
    return RMW_RET_INVALID_ARGUMENT;
  }
  RCUTILS_CHECK_ALLOCATOR_WITH_MSG(
    allocator, "allocator argument is invalid", return RMW_RET_INVALID_ARGUMENT);
  if (RMW_RET_OK != rmw_topic_endpoint_info_array_check_zero(subscriptions_info)) {
    return RMW_RET_INVALID_ARGUMENT;
  }
  (void)no_mangle;

  RMW_SET_ERROR_MSG("rmw_get_subscriptions_info_by_topic hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
