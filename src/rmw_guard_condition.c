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

#include <stdatomic.h>

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Creates guard condition. data element isn't even pointer, it's just a counter variable
rmw_guard_condition_t *
rmw_create_guard_condition(
  rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_INVALID_ARGUMENT);

  rmw_guard_condition_t * guard = rmw_guard_condition_allocate();
  if (guard == NULL) {
    RMW_SET_ERROR_MSG("failed to allocate memory for guard condition");
    return NULL;
  }
  guard->implementation_identifier = rmw_get_implementation_identifier();
  guard->data = (void*)0L;
  guard->context = context;

  return guard;
}

rmw_ret_t
rmw_destroy_guard_condition(
  rmw_guard_condition_t * guard_condition)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_condition, RMW_RET_INVALID_ARGUMENT);

  rmw_free(guard_condition);

  return RMW_RET_OK;
}

rmw_ret_t
rmw_trigger_guard_condition(
  const rmw_guard_condition_t * guard_condition)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_condition, RMW_RET_INVALID_ARGUMENT);

  long * counter = (long*)guard_condition->data;
  *counter++;

  return RMW_RET_OK;
}

#ifdef __cplusplus
}
#endif