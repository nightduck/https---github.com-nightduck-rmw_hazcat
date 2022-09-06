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

#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_hazcat/guard_condition.h"

#ifdef __cplusplus
extern "C"
{
#endif

char dummy[4096];

rmw_ret_t
create_guard_condition_impl(
  guard_condition_t * gc)
{
  if (0 != pipe2(gc->pfd, O_NONBLOCK)) {
    RMW_SET_ERROR_MSG("failed to create pipe for guard condition");
    return RMW_RET_ERROR;
  }
  gc->ev.events = EPOLLIN;
  gc->ev.data.fd = gc->pfd[GC_FD_READ];

  return RMW_RET_OK;
}

rmw_ret_t
destroy_guard_condition_impl(
  guard_condition_t * gc)
{
  // close(gc->pfd[GC_FD_READ]);
  // close(gc->pfd[GC_FD_WRITE]);

  return RMW_RET_OK;
}

int
guard_condition_trigger_count(
  guard_condition_t * gc)
{
  int ret = read(gc->pfd[GC_FD_READ], dummy, 4096);
  if (-1 == ret && EAGAIN != errno) {
    perror("read");
    return 0;
  } else if (-1 == ret && EAGAIN == errno) {
    return 0;
  } else {
    return ret;
  }
}

// Utility method to copy guard condition and implementation into specified location. Used when
// guard condition needs to be in shared memory
rmw_ret_t
copy_guard_condition(
  rmw_guard_condition_t * dest,
  guard_condition_t * dest_impl,
  rmw_guard_condition_t * src)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(dest, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(dest_impl, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(src, RMW_RET_INVALID_ARGUMENT);

  dest->implementation_identifier = src->implementation_identifier;
  dest->context = src->context;
  dest->data = (void *)dest_impl;
  dest_impl->ev = ((guard_condition_t *)src->data)->ev;
  dest_impl->pfd[0] = ((guard_condition_t *)src->data)->pfd[0];
  dest_impl->pfd[1] = ((guard_condition_t *)src->data)->pfd[1];

  return RMW_RET_OK;
}

#ifdef __cplusplus
}
#endif
