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

#ifndef RMW_HAZCAT__HAZCAT_NODE_H_
#define RMW_HAZCAT__HAZCAT_NODE_H_

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct hazcat_node_info
{
  rmw_guard_condition_t * const guard_condition_;   // Triggers whenver ros graph changes
} node_info_t;

// Bytewise identical with above but with const keywords removed for one time assignment
typedef struct hazcat_node_info__
{
  rmw_guard_condition_t * guard_condition;
} construct_node_info__;

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__HAZCAT_NODE_H_
