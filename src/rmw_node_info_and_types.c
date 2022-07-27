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

#include "rcutils/error_handling.h"
#include "rcutils/logging_macros.h"
#include "rcutils/strdup.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"
#include "rmw/get_node_info_and_types.h"
#include "rmw/get_service_names_and_types.h"
#include "rmw/get_topic_names_and_types.h"
#include "rmw/names_and_types.h"

#ifdef __cplusplus
extern "C"
{
#endif
rmw_ret_t
rmw_get_subscriber_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_name, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespace, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_names_and_types, RMW_RET_ERROR);
  (void) no_demangle;

  RMW_SET_ERROR_MSG("rmw_get_subscriber_names_and_types_by_node hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_publisher_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_name, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespace, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_names_and_types, RMW_RET_ERROR);
  (void) no_demangle;

  RMW_SET_ERROR_MSG("rmw_get_publisher_names_and_types_by_node hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_service_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  rmw_names_and_types_t * service_names_and_types)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_name, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespace, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(service_names_and_types, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_get_service_names_and_types_by_node hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_service_names_and_types(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  rmw_names_and_types_t * service_names_and_types)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(service_names_and_types, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_get_service_names_and_types hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_client_names_and_types_by_node(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  const char * node_name,
  const char * node_namespace,
  rmw_names_and_types_t * service_names_and_types)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_name, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespace, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(service_names_and_types, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_get_client_names_and_types_by_node hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_topic_names_and_types(
  const rmw_node_t * node,
  rcutils_allocator_t * allocator,
  bool no_demangle,
  rmw_names_and_types_t * topic_names_and_types)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(allocator, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(topic_names_and_types, RMW_RET_ERROR);
  (void)no_demangle;

  RMW_SET_ERROR_MSG("rmw_get_topic_names_and_types hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
