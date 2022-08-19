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
rmw_compare_gids_equal(
  const rmw_gid_t * gid1,
  const rmw_gid_t * gid2,
  bool * result)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(gid1, RMW_RET_INVALID_ARGUMENT);
  if (gid1->implementation_identifier != rmw_get_implementation_identifier()) {
    RMW_SET_ERROR_MSG("Provided gid does not match expected implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(gid2, RMW_RET_INVALID_ARGUMENT);
  if (gid2->implementation_identifier != rmw_get_implementation_identifier()) {
    RMW_SET_ERROR_MSG("Provided gid does not match expected implementation");
    return RMW_RET_INCORRECT_RMW_IMPLEMENTATION;
  }
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(result, RMW_RET_INVALID_ARGUMENT);
  *result = false;

  if (memcmp(gid1->data, gid2->data, RMW_GID_STORAGE_SIZE) == 0) {
    *result = true;
  }
  return RMW_RET_OK;
}
#ifdef __cplusplus
}
#endif
