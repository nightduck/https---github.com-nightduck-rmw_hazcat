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

#include "rmw_hazcat/types.h"
#include "rmw_hazcat/guard_condition.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Creates guard condition
rmw_guard_condition_t *
rmw_create_guard_condition(
  rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, NULL);

  rmw_guard_condition_t * guard = rmw_guard_condition_allocate();
  if (guard == NULL) {
    RMW_SET_ERROR_MSG("failed to allocate memory for guard condition");
    return NULL;
  }
  guard->implementation_identifier = rmw_get_implementation_identifier();
  guard->context = context;

  guard_condition_t * gc_impl = rmw_allocate(sizeof(guard_condition_t));
  create_guard_condition_impl(gc_impl);
  guard->data = gc_impl;

  return guard;
}

rmw_ret_t
rmw_destroy_guard_condition(
  rmw_guard_condition_t * guard_condition)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_condition, RMW_RET_INVALID_ARGUMENT);

  guard_condition_t * gc = (guard_condition_t *)guard_condition->data;

  destroy_guard_condition_impl(gc);
  rmw_free(guard_condition->data);
  rmw_free(guard_condition);

  return RMW_RET_OK;
}

// Triggers guard condition. Note, in this rmw, guards may be created in other processes, so
// the implementation and context pointers may not be valid. Don't rely on them
rmw_ret_t
rmw_trigger_guard_condition(
  const rmw_guard_condition_t * guard_condition)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(guard_condition, RMW_RET_INVALID_ARGUMENT);

  // Write one byte to the pipe owned by guard condition, sending a signal to anyone waiting on it
  guard_condition_t * gc = guard_condition->data;
  uint8_t dummy = 0x1;
  if (write(gc->pfd[1], &dummy, 1) < 1) {
    RMW_SET_ERROR_MSG("Error triggering guard condition");
    return RMW_RET_ERROR;
  }

  return RMW_RET_OK;
}

#ifdef __cplusplus
}
#endif
