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

#include "rmw_hazcat/hazcat_message_queue.h"

#ifdef __cplusplus
extern "C"
{
#endif
rmw_ret_t
rmw_init_options_init(rmw_init_options_t * init_options, rcutils_allocator_t allocator)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_ERROR);
  (void)allocator;

  RMW_SET_ERROR_MSG("rmw_init_options_init hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_init_options_copy(const rmw_init_options_t * src, rmw_init_options_t * dst)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(src, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(dst, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_init_options_copy hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_init_options_fini(rmw_init_options_t * init_options)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_init_options_fini hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_init(const rmw_init_options_t * options, rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(options, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_ERROR);

  #ifdef CUDA
  CHECK_DRV(cuInit(0));
  #endif

  return hazcat_init();
}

rmw_ret_t
rmw_shutdown(rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_ERROR);

  return hazcat_fini();
}

rmw_ret_t
rmw_context_fini(rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_ERROR);

  RMW_SET_ERROR_MSG("rmw_context_fini hasn't been implemented yet");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
