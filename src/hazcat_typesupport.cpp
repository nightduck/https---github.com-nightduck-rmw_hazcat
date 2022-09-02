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

#include "rosidl_runtime_c/string_functions.h"

#include "rosidl_typesupport_c/message_type_support_dispatch.h"

#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"

#include "rosidl_typesupport_introspection_cpp/field_types.hpp"
#include "rosidl_typesupport_introspection_cpp/identifier.hpp"
#include "rosidl_typesupport_introspection_cpp/message_introspection.hpp"

#define RMW_HAZCAT_TYPESUPPORT_C    rosidl_typesupport_introspection_c__identifier
#define RMW_HAZCAT_TYPESUPPORT_CPP  rosidl_typesupport_introspection_cpp::typesupport_identifier

extern "C"
const rosidl_message_type_support_t *
get_type_support(
  const rosidl_message_type_support_t * type_support);

const rosidl_message_type_support_t *
get_type_support(
  const rosidl_message_type_support_t * type_support)
{
  const rosidl_message_type_support_t * ts_c =
    reinterpret_cast<const rosidl_message_type_support_t *>(
    type_support->func(type_support, RMW_HAZCAT_TYPESUPPORT_C));
  if (ts_c) {
    return ts_c;
  }
  const rosidl_message_type_support_t * ts_cpp =
    reinterpret_cast<const rosidl_message_type_support_t *>(
    type_support->func(type_support, RMW_HAZCAT_TYPESUPPORT_CPP));
  if (ts_cpp) {
    return ts_cpp;
  }

  RMW_SET_ERROR_MSG("Unsupported typesupport");
  return nullptr;
}
