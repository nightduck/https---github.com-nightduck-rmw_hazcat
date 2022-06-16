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
rmw_serialize(
  const void * ros_message,
  const rosidl_message_type_support_t * type_supports,
  rmw_serialized_message_t * serialized_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_serialize hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
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

  RMW_SET_ERROR_MSG("rmw_get_serialized_message_size hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif