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

#ifndef RMW_HAZCAT__GUARD_CONDITION_H_
#define RMW_HAZCAT__GUARD_CONDITION_H_

#ifdef __cplusplus
# include <atomic>
# define _Atomic(X) std::atomic < X >
using std::atomic;
using std::atomic_int;
using std::atomic_uint_fast32_t;
extern "C"
{
#else
# include <stdatomic.h>
#endif
#include <fcntl.h>
#include <unistd.h>

#include <stdint.h>
#include "rmw/allocators.h"
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_hazcat/types.h"

rmw_ret_t
create_guard_condition_impl(
  guard_condition_t * gc);

rmw_ret_t
destroy_guard_condition_impl(
  guard_condition_t * gc);

int
guard_condition_trigger_count(
  guard_condition_t * gc);

rmw_ret_t
copy_guard_condition(
  rmw_guard_condition_t * dest,
  guard_condition_t * dest_impl,
  rmw_guard_condition_t * src);

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__GUARD_CONDITION_H_
