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

#include "rmw_hazcat/hazcat_node.h"

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

  if (context->implementation_identifier != rmw_get_implementation_identifier()) {
    return NULL;
  }

  rmw_node_t * node = rmw_node_allocate();
  if (node == NULL) {
    RMW_SET_ERROR_MSG("failed to allocate memory for node handle");
    return NULL;
  }
  node->implementation_identifier = rmw_get_implementation_identifier();

  node->data = rmw_allocate(sizeof(node_info_t));
  ((construct_node_info__*)node->data)->guard_condition = rmw_create_guard_condition(context);

  node->name = rmw_allocate(strlen(name) + 1);
  if (node == NULL) {
    RMW_SET_ERROR_MSG("failed to allocate memory for node name string");
    rmw_free(node);
    return NULL;
  }
  snprintf(node->name, strlen(name) + 1, name);

  node->namespace_ = rmw_allocate(strlen(namespace_) + 1);
  if (node == NULL) {
    RMW_SET_ERROR_MSG("failed to allocate memory for node namespace string");
    rmw_free(node->name);
    rmw_free(node);
    return NULL;
  }
  snprintf(node->namespace_, strlen(namespace_) + 1, namespace_);

  node->context = context;

  return node;
}

rmw_ret_t
rmw_destroy_node(rmw_node_t * node)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_INVALID_ARGUMENT);

  rmw_free(node->namespace_);
  rmw_free(node->name);
  rmw_free(node->data);
  rmw_free(node);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_node_assert_liveliness(const rmw_node_t * node)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_INVALID_ARGUMENT);

  RMW_SET_ERROR_MSG("rmw_node_assert_liveliness has been deprecated");
  return RMW_RET_UNSUPPORTED;
}

const rmw_guard_condition_t *
rmw_node_get_graph_guard_condition(const rmw_node_t * node)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, NULL);

  return ((node_info_t*)node->data)->guard_condition_;
}

rmw_ret_t
rmw_get_node_names(
  const rmw_node_t * node,
  rcutils_string_array_t * node_names,
  rcutils_string_array_t * node_namespaces)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_names, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespaces, RMW_RET_INVALID_ARGUMENT);

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
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_names, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(node_namespaces, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(enclaves, RMW_RET_INVALID_ARGUMENT);

  RMW_SET_ERROR_MSG("rmw_node_get_graph_guard_condition hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
