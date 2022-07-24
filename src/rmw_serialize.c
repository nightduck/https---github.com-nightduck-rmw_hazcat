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
rmw_serialize(
  const void * ros_message,
  const rosidl_message_type_support_t * type_supports,
  rmw_serialized_message_t * serialized_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_ERROR);
  rmw_ret_t ret;

  // TODO: This is a bunch of pseudocode referencing nonexistant functions, fill in gaps
  if (type_supports->fixed_size) {
    if (ret = rmw_serialized_message_resize(serialized_message, type_supports->message_size)
      != RMW_RET_OK)
    {
      RMW_SET_ERROR_MSG("Cannot resize serialized message");
      return ret;
    }
    memcpy(serialized_message, ros_message, type_supports->message_size);
    return RMW_RET_OK;
  }

  size_t len;
  if (ret = rmw_get_serialized_message_size(type_supports, ???, &len) != RMW_RET_OK) {
    RMW_SET_ERROR_MSG("Unable to get size of serialized message");
    return ret;
  }
  rmw_serialized_message_resize(serialized_message, len);
  serialize_message(serialized_message, ros_message, type_supports);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_deserialize(
  const rmw_serialized_message_t * serialized_message,
  const rosidl_message_type_support_t * type_supports,
  void * ros_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_deserialize hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_serialized_message_size(
  const rosidl_message_type_support_t * type_supports,
  const rosidl_runtime_c__Sequence__bound * message_bounds,
  size_t * size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_bounds, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(size, RMW_RET_ERROR);

  rosidl_message_type_support * ts_c = get_message_typesupport_handle(
    type_supports, rosidl_typesupport_introspection_c__identifier);

  const rosidl_typesupport_introspection_c__MessageMembers * members = 
    (const rosidl_typesupport_introspection_c__MessageMembers *)ts_c->data;

  return members->size_of_;
}
#ifdef __cplusplus
}
#endif
