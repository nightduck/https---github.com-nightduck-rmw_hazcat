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
#include "rmw/types.h"

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
  if (NULL != init_options->implementation_identifier) {
    RMW_SET_ERROR_MSG("expected zero-initialized init_options");
    return RMW_RET_INVALID_ARGUMENT;
  }
  init_options->instance_id = 0;
  init_options->implementation_identifier = rmw_get_implementation_identifier();
  init_options->allocator = allocator;
  init_options->impl = NULL;
  init_options->security_options = rmw_get_zero_initialized_security_options();
  init_options->domain_id = RMW_DEFAULT_DOMAIN_ID;
  init_options->localhost_only = RMW_LOCALHOST_ONLY_ENABLED;
  init_options->enclave = NULL;

  return RMW_RET_OK;
}

rmw_ret_t
rmw_init_options_copy(const rmw_init_options_t * src, rmw_init_options_t * dst)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(src, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(dst, RMW_RET_ERROR);
  if (src->implementation_identifier == rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  if (NULL != dst->implementation_identifier) {
    RMW_SET_ERROR_MSG("expected zero-initialized dst");
    return RMW_RET_INVALID_ARGUMENT;
  }
  *dst = *src;

  return RMW_RET_OK;
}

rmw_ret_t
rmw_init_options_fini(rmw_init_options_t * init_options)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(init_options, RMW_RET_ERROR);
  RCUTILS_CHECK_ALLOCATOR(&(init_options->allocator), return RMW_RET_INVALID_ARGUMENT);
  if (init_options->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  const rcutils_allocator_t * allocator = &init_options->allocator;
  RCUTILS_CHECK_ALLOCATOR(allocator, return RMW_RET_INVALID_ARGUMENT);

  allocator->deallocate(init_options->enclave, allocator->state);
  rmw_ret_t ret = rmw_security_options_fini(&init_options->security_options, allocator);
  *init_options = rmw_get_zero_initialized_init_options();
  return ret;
}

rmw_ret_t
rmw_init(const rmw_init_options_t * options, rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(options, RMW_RET_ERROR);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_ERROR);
  if (options->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  #ifdef _LINUX_CUDA_H
  CHECK_DRV(cuInit(0));
  #endif

  context->instance_id = options->instance_id;
  context->implementation_identifier = rmw_get_implementation_identifier();
  context->impl = NULL;
  context->options = *options;

  return hazcat_init();
}

rmw_ret_t
rmw_shutdown(rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_ERROR);
  if (context->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  return hazcat_fini();
}

rmw_ret_t
rmw_context_fini(rmw_context_t * context)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(context, RMW_RET_ERROR);
  if (context->implementation_identifier != rmw_get_implementation_identifier()) {
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }

  rmw_ret_t ret = rmw_init_options_fini(&context->options);

  // context impl is explicitly supposed to be nullptr for now, see rmw_init()
  *context = rmw_get_zero_initialized_context();
  return ret;
}
#ifdef __cplusplus
}
#endif
