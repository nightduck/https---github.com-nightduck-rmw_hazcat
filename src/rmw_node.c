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
rmw_node_t *
rmw_create_node(
  rmw_context_t * context,
  const char * name,
  const char * namespace_,
  size_t domain_id,
  bool localhost_only)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(name, NULL);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(namespace_, NULL);
  (void)domain_id;
  (void)localhost_only;

  RMW_SET_ERROR_MSG("rmw_create_node hasn't been implemented yet");
  return NULL;
}

rmw_ret_t
rmw_destroy_node(rmw_node_t * node)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_destroy_node hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_node_assert_liveliness(const rmw_node_t * node)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_node_assert_liveliness hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

const rmw_guard_condition_t *
rmw_node_get_graph_guard_condition(const rmw_node_t * node)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, NULL);

  RMW_SET_ERROR_MSG("rmw_node_get_graph_guard_condition hasn't been implemented yet");
  return NULL;
}

rmw_ret_t
rmw_get_node_names(
  const rmw_node_t * node,
  rcutils_string_array_t * node_names,
  rcutils_string_array_t * node_namespaces)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_names, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespaces, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_node_get_graph_guard_condition hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_node_names_with_enclaves(
  const rmw_node_t * node,
  rcutils_string_array_t * node_names,
  rcutils_string_array_t * node_namespaces,
  rcutils_string_array_t * enclaves)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_names, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespaces, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(enclaves, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_node_get_graph_guard_condition hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
