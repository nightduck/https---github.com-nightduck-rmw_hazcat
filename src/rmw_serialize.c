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

#include <string.h>
#include <stdio.h>

#include <ucdr/microcdr.h>

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rosidl_runtime_c/string_functions.h"

#include "rosidl_typesupport_c/identifier.h"

#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"

// #include "rmw_hazcat/hashtable.h"

#define RMW_HAZCAT_TYPESUPPORT_C    rosidl_typesupport_introspection_c__identifier
// #define RMW_HAZCAT_TYPESUPPORT_CPP  rosidl_typesupport_cpp__typesupport_identifier

#ifdef __cplusplus
extern "C"
{
#endif

rmw_ret_t
serialize(
  const void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members,
  rmw_serialized_message_t * serialized_msg,
  ucdrBuffer * writer)
{
  assert(members);
  assert(ros_message);

  for (uint32_t i = 0; i < members->member_count_; i++) {
    const rosidl_typesupport_introspection_c__MessageMember * member = members->members_ + i;
    const char * ros_message_field = (const char *)(ros_message) + member->offset_;
    const rosidl_message_type_support_t * ts;
    const rosidl_typesupport_introspection_c__MessageMembers * sub_members;
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        ts = member->members_;
        sub_members = (const rosidl_typesupport_introspection_c__MessageMembers *)ts->data;

        // Recurse
        serialize(ros_message_field, sub_members, serialized_msg, writer);
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
        if (member->is_array_) {
          ucdr_serialize_array_bool(writer, (bool *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(bool);
        } else {
          ucdr_serialize_bool(writer, *(bool *)ros_message_field);
          serialized_msg->buffer_length += sizeof(bool);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
        if (member->is_array_) {
          ucdr_serialize_array_char(writer, (char *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(char);
        } else {
          ucdr_serialize_char(writer, *(char *)ros_message_field);
          serialized_msg->buffer_length += sizeof(char);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
        if (member->is_array_) {
          ucdr_serialize_array_uint8_t(writer, (uint8_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(uint8_t);
        } else {
          ucdr_serialize_uint8_t(writer, *(uint8_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(uint8_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
        if (member->is_array_) {
          ucdr_serialize_array_int8_t(writer, (int8_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(int8_t);
        } else {
          ucdr_serialize_int8_t(writer, *(int8_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(int8_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT32:
        if (member->is_array_) {
          ucdr_serialize_array_float(writer, (float *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(float);
        } else {
          ucdr_serialize_float(writer, *(float *)ros_message_field);
          serialized_msg->buffer_length += sizeof(float);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT64:
        if (member->is_array_) {
          ucdr_serialize_array_double(writer, (double *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(double);
        } else {
          ucdr_serialize_double(writer, *(double *)ros_message_field);
          serialized_msg->buffer_length += sizeof(double);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
        if (member->is_array_) {
          ucdr_serialize_array_int16_t(writer, (int16_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(int16_t);
        } else {
          ucdr_serialize_int16_t(writer, *(int16_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(int16_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
        if (member->is_array_) {
          ucdr_serialize_array_uint16_t(writer, (uint16_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(uint16_t);
        } else {
          ucdr_serialize_uint16_t(writer, *(uint16_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(uint16_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
        if (member->is_array_) {
          ucdr_serialize_array_int32_t(writer, (int32_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(int32_t);
        } else {
          ucdr_serialize_int32_t(writer, *(int32_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(int32_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
        if (member->is_array_) {
          ucdr_serialize_array_uint32_t(writer, (uint32_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(uint32_t);
        } else {
          ucdr_serialize_uint32_t(writer, *(uint32_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(uint32_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
        if (member->is_array_) {
          ucdr_serialize_array_int64_t(writer, (int64_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(int64_t);
        } else {
          ucdr_serialize_int64_t(writer, *(int64_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(int64_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
        if (member->is_array_) {
          ucdr_serialize_array_uint64_t(writer, (uint64_t *)ros_message_field, member->array_size_);
          serialized_msg->buffer_length += member->array_size_ * sizeof(uint64_t);
        } else {
          ucdr_serialize_uint64_t(writer, *(uint64_t *)ros_message_field);
          serialized_msg->buffer_length += sizeof(uint64_t);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        if (member->is_array_) {
          ucdr_serialize_string(writer, (char *)ros_message_field);
          serialized_msg->buffer_length += member->string_upper_bound_;
        }
        break;
      default:
        RMW_SET_ERROR_MSG("Serializing unknown type");
        return RMW_RET_INVALID_ARGUMENT;
    }
  }

  return RMW_RET_OK;
}

rmw_ret_t
deserialize(
  void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members,
  const void * serialized_msg,
  ucdrBuffer * reader)
{
  assert(members);
  assert(serialized_msg);

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const rosidl_typesupport_introspection_c__MessageMember * member = members->members_ + i;
    char * ros_message_field = (char *)(ros_message) + member->offset_;
    const rosidl_message_type_support_t * ts;
    const rosidl_typesupport_introspection_c__MessageMembers * sub_members;
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        ts = member->members_;
        sub_members = (const rosidl_typesupport_introspection_c__MessageMembers *)ts->data;

        // Recurse
        deserialize(ros_message_field, sub_members, serialized_msg, reader);
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
        if (member->is_array_) {
          ucdr_deserialize_array_bool(reader, (bool *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_bool(reader, (bool *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
        if (member->is_array_) {
          ucdr_deserialize_array_char(reader, (char *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_char(reader, (char *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
        if (member->is_array_) {
          ucdr_deserialize_array_uint8_t(reader, (uint8_t *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_uint8_t(reader, (uint8_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
        if (member->is_array_) {
          ucdr_deserialize_array_int8_t(reader, (int8_t *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_int8_t(reader, (int8_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT32:
        if (member->is_array_) {
          ucdr_deserialize_array_float(reader, (float *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_float(reader, (float *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT64:
        if (member->is_array_) {
          ucdr_deserialize_array_double(reader, (double *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_double(reader, (double *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
        if (member->is_array_) {
          ucdr_deserialize_array_int16_t(reader, (int16_t *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_int16_t(reader, (int16_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
        if (member->is_array_) {
          ucdr_deserialize_array_uint16_t(
            reader, (uint16_t *)ros_message_field,
            member->array_size_);
        } else {
          ucdr_deserialize_uint16_t(reader, (uint16_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
        if (member->is_array_) {
          ucdr_deserialize_array_int32_t(reader, (int32_t *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_int32_t(reader, (int32_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
        if (member->is_array_) {
          ucdr_deserialize_array_uint32_t(
            reader, (uint32_t *)ros_message_field,
            member->array_size_);
        } else {
          ucdr_deserialize_uint32_t(reader, (uint32_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
        if (member->is_array_) {
          ucdr_deserialize_array_int64_t(reader, (int64_t *)ros_message_field, member->array_size_);
        } else {
          ucdr_deserialize_int64_t(reader, (int64_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
        if (member->is_array_) {
          ucdr_deserialize_array_uint64_t(
            reader, (uint64_t *)ros_message_field,
            member->array_size_);
        } else {
          ucdr_deserialize_uint64_t(reader, (uint64_t *)ros_message_field);
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        if (member->is_array_) {
          ucdr_deserialize_string(reader, (char *)ros_message_field, member->string_upper_bound_);
        }
        break;
      default:
        RMW_SET_ERROR_MSG("Serializing unknown type");
        return RMW_RET_INVALID_ARGUMENT;
    }
  }

  return RMW_RET_OK;
}

rmw_ret_t
rmw_serialize(
  const void * ros_message,
  const rosidl_message_type_support_t * type_support,
  rmw_serialized_message_t * serialized_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);

  rosidl_message_type_support_t * ts_c =
    (rosidl_message_type_support_t *)type_support->func(type_support, RMW_HAZCAT_TYPESUPPORT_C);
  if (ts_c == NULL) {
    RMW_SET_ERROR_MSG("rmw_hazcat only supports rosidl_typesupport_introspection_c");
    return RMW_RET_INVALID_ARGUMENT;
  }
  rosidl_typesupport_introspection_c__MessageMembers * members =
    (rosidl_typesupport_introspection_c__MessageMembers *)ts_c->data;
  rmw_ret_t ret;
  if ((ret = rmw_serialized_message_resize(serialized_message, members->size_of_)) != RMW_RET_OK) {
    RMW_SET_ERROR_MSG("Cannot resize serialized message");
    return ret;
  }

  // CDR buffer
  ucdrBuffer writer;
  ucdr_init_buffer(&writer, serialized_message->buffer, members->size_of_);

  // Serialize the message
  return serialize(ros_message, members, serialized_message, &writer);
}

rmw_ret_t
rmw_deserialize(
  const rmw_serialized_message_t * serialized_message,
  const rosidl_message_type_support_t * type_support,
  void * ros_message)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);

  rosidl_message_type_support_t * ts_c =
    (rosidl_message_type_support_t *)type_support->func(type_support, RMW_HAZCAT_TYPESUPPORT_C);
  if (ts_c == NULL) {
    RMW_SET_ERROR_MSG("rmw_hazcat only supports rosidl_typesupport_introspection_c");
    return RMW_RET_INVALID_ARGUMENT;
  }
  rosidl_typesupport_introspection_c__MessageMembers * members =
    (rosidl_typesupport_introspection_c__MessageMembers *)ts_c->data;
  rmw_ret_t ret;
  if ((ret = rmw_serialized_message_resize(serialized_message, members->size_of_)) != RMW_RET_OK) {
    RMW_SET_ERROR_MSG("Cannot resize serialized message");
    return ret;
  }

  // CDR buffer
  ucdrBuffer reader;
  ucdr_init_buffer(&reader, serialized_message->buffer, members->size_of_);

  // Serialize the message
  return deserialize(ros_message, members, serialized_message, &reader);
}

rmw_ret_t
rmw_get_serialized_message_size(
  const rosidl_message_type_support_t * type_support,
  const rosidl_runtime_c__Sequence__bound * message_bounds,
  size_t * size)
{
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_support, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_bounds, RMW_RET_INVALID_ARGUMENT);
  RCUTILS_CHECK_ARGUMENT_FOR_NULL(size, RMW_RET_INVALID_ARGUMENT);

  rosidl_message_type_support_t * ts_c =
    (rosidl_message_type_support_t *)type_support->func(type_support, RMW_HAZCAT_TYPESUPPORT_C);
  if (ts_c == NULL) {
    RMW_SET_ERROR_MSG("rmw_hazcat only supports rosidl_typesupport_introspection_c");
    return RMW_RET_INVALID_ARGUMENT;
  }
  rosidl_typesupport_introspection_c__MessageMembers * members =
    (rosidl_typesupport_introspection_c__MessageMembers *)ts_c->data;
  if (members == NULL) {
    RMW_SET_ERROR_MSG("error reading introspection for message");
    return RMW_RET_INVALID_ARGUMENT;
  }
  *size = members->size_of_;

  return RMW_RET_OK;
}
#ifdef __cplusplus
}
#endif
