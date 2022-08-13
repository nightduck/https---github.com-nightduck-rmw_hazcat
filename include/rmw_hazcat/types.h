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

#ifndef RMW_HAZCAT__TYPES_H_
#define RMW_HAZCAT__TYPES_H_


#ifdef __cplusplus
extern "C"
{
#endif

typedef struct waitset {
  size_t num_subs;
  size_t num_gcs;
  size_t num_cls;
  size_t num_srv;
  size_t num_evt;
  rmw_subscription_t ** subscriptions;
  rmw_guard_condition_t ** guard_conditions;
  rmw_client_t ** clients;
  rmw_service_t ** services;
  rmw_events_t ** events;
} waitset_t;

#ifdef __cplusplus
}
#endif

#endif  // RMW_HAZCAT__TYPES_H_