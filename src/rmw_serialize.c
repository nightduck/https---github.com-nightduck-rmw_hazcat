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
#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rosidl_runtime_c/string_functions.h"

#include "rosidl_typesupport_c/identifier.h"

#include "rosidl_typesupport_introspection_c/field_types.h"
#include "rosidl_typesupport_introspection_c/identifier.h"
#include "rosidl_typesupport_introspection_c/message_introspection.h"

//#include "rosidl_typesupport_cpp/identifier.hpp"

//#include "rmw_hazcat/hashtable.h"

#define RMW_HAZCAT_TYPESUPPORT_C    rosidl_typesupport_c__typesupport_identifier
//#define RMW_HAZCAT_TYPESUPPORT_CPP  rosidl_typesupport_cpp__typesupport_identifier

#ifdef __cplusplus
extern "C"
{
#endif

// Hash table linking all known message types for quick lookup
//hashtable_t * cache;

bool is_complex_type(const rosidl_typesupport_introspection_c__MessageMember * member)
{
  return member->type_id_ == rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE;
}

bool is_vector_type(const rosidl_typesupport_introspection_c__MessageMember * member)
{
  return member->type_id_ == rosidl_typesupport_introspection_c__ROS_TYPE_STRING ||
         (member->is_array_ && !(member->array_size_ > 0 && !member->is_upper_bound_));
}

bool is_fixed_size(const rosidl_typesupport_introspection_c__MessageMembers * members)
{
  char * name = rmw_allocate(strlen(members->message_namespace_) + strlen(members->message_name_));
  snprintf(name, strlen(members->message_namespace_) + strlen(members->message_name_), "%s%s",
    members->message_namespace_, members->message_name_);

  // TODO: Lookup type in cache and return stored boolean value if present

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const rosidl_typesupport_introspection_c__MessageMember * member = members->members_ + i;
    // if ROS type, call recursively
    if (is_complex_type(member)) {
      bool result = is_fixed_size(
        (const rosidl_typesupport_introspection_c__MessageMembers *)member->members_->data);
      // early exit
      if (!result) {
        //cache.insert({name, result});
        // TODO: Insert name and "false" into cache
        return false;
      }
    } else if (is_vector_type(member)) {
      //cache.insert({name, false});
      // TODO: Insert name and "false" into cache
      return false;
    }
  }

  //cache.insert({name, true});
  // TODO: Insert name and "true" into cache
  return true;
}

void serialize_element(
  char * serialized_msg,
  const char * ros_message_field,
  size_t size)
{
  //debug_log("serializing data element of %u bytes\n", size);
  memcpy(serialized_msg, ros_message_field, size);
  serialized_msg += size;
}

const char * deserialize_element(
  const char * serialized_msg,
  void * ros_message_field,
  size_t size)
{
  memcpy(ros_message_field, serialized_msg, size);
  serialized_msg += size;

  return serialized_msg;
}

typedef struct rosidl_runtime_c__Sequence
{
  void * data;      /*!< The pointer to an array of STRUCT_NAME */
  size_t size;      /*!< The number of valid items in data */
  size_t capacity;  /*!< The number of allocated items in data */
} rosidl_runtime_c__Sequence;

void serialize_sequence(char * serialized_msg, const char * ros_message_field, size_t size)
{
  rosidl_runtime_c__Sequence * sequence = (rosidl_runtime_c__Sequence *)(ros_message_field);
  uint32_t sequence_size = sequence->size;
  const uint32_t check = 101;
  uint32_t seq_size[2] = {check, sequence_size};

  serialize_element(serialized_msg, seq_size, 2 * sizeof(uint32_t));

  serialize_element(serialized_msg, (const char *)(sequence->data),
    sequence_size * size);
}

const char * deserialize_sequence(const char * serialized_msg, void * ros_message_field, size_t size)
{
  uint32_t array_size = 0;
  const uint32_t check = 101;
  uint32_t seq_size[2] = {check, array_size};

  deserialize_element(serialized_msg, seq_size, 2 * sizeof(uint32_t));
  array_size = seq_size[1];

  if (array_size > 0) {
    rosidl_runtime_c__Sequence * sequence = (rosidl_runtime_c__Sequence *)(ros_message_field);
    sequence->data = rmw_allocate(array_size * size);
    sequence->size = array_size;
    sequence->capacity = array_size;

    return deserialize_element(serialized_msg, sequence->data, array_size * size);
  }
  return serialized_msg;
}

void serialize_message_field(
  const rosidl_typesupport_introspection_c__MessageMember * member,
  char * serialized_msg,
  const char * ros_message_field,
  size_t size)
{
  //debug_log("serializing message field %s\n", member->name_);
  if (!member->is_array_) {
    serialize_element(serialized_msg, ros_message_field, size);
  } else if (member->array_size_ > 0 && !member->is_upper_bound_) {
    serialize_element(serialized_msg, ros_message_field, size * member->array_size_);
  } else {
    serialize_sequence(serialized_msg, ros_message_field, size);
  }
}

const char * deserialize_message_field(
  const rosidl_typesupport_introspection_c__MessageMember * member,
  const char * serialized_msg, void * ros_message_field, size_t size)
{
  //debug_log("deserializing %s\n", member->name_);
  if (!member->is_array_) {
    return deserialize_element(serialized_msg, ros_message_field, size);
  } else if (member->array_size_ > 0 && !member->is_upper_bound_) {
    return deserialize_element(serialized_msg, ros_message_field, size * member->array_size_);
  } else {
    return deserialize_sequence(serialized_msg, ros_message_field, size);
  }
}

void serialize(
  const void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members,
  void * serialized_msg)
{
  assert(members);
  assert(ros_message);

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const rosidl_typesupport_introspection_c__MessageMember * member = members->members_ + i;
    const char * ros_message_field = (const char *)(ros_message) + member->offset_;
    const rosidl_runtime_c__String * string = (const rosidl_runtime_c__String *)(ros_message_field);
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        {
          // Iterate recursively over the complex ROS messages
          const rosidl_typesupport_introspection_c__MessageMembers * sub_members =
            (const rosidl_typesupport_introspection_c__MessageMembers *)(member->members_
            ->data);

          const void * subros_message = NULL;
          size_t sequence_size = 0;
          size_t sub_members_size = sub_members->size_of_;
          // It's a single message
          if (!member->is_array_) {
            subros_message = ros_message_field;
            sequence_size = 1;
            // It's a fixed size array of messages
          } else if (member->array_size_ > 0 && !member->is_upper_bound_) {
            subros_message = ros_message_field;
            sequence_size = member->array_size_;
            // It's a dynamic sequence of messages
          } else {
            // Cast current ros_message_field ptr as vector "definition"
            const rosidl_runtime_c__char__Sequence * vector =
              (const rosidl_runtime_c__char__Sequence *)(ros_message_field);
            // Vector size points to content of vector and returns number of bytes
            // submembersize is the size of one element in the vector
            // (it is provided by type support)
            sequence_size = vector->size / sub_members_size;
            if (member->is_upper_bound_ && sequence_size > member->array_size_) {
              RMW_SET_ERROR_MSG("vector overcomes the maximum length");
              return RMW_RET_ERROR;
            }
            // create ptr to content of vector to enable recursion
            subros_message = (const void *)(vector->data);
            const uint32_t check = 101;
            uint32_t seq_size[2] = {check, sequence_size};

            serialize_element(serialized_msg, seq_size, 2 * sizeof(uint32_t));
          }

          //debug_log("serializing message field %s\n", member->name_);
          for (auto index = 0u; index < sequence_size; ++index) {
            serialize(subros_message, sub_members, serialized_msg);
            subros_message = (const char *)(subros_message) + sub_members_size;
          }
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT64:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
        serialize_message_field(member, serialized_msg, ros_message_field,
          member->size_function(ros_message_field));
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        serialize_message_field(member, serialized_msg, ros_message_field,
          string->size);
        break;
      default:
        RMW_SET_ERROR_MSG("Serializing unknown type");
        return RMW_RET_INVALID_ARGUMENT;
    }
  }
}

void deserialize(
  const void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members,
  void * serialized_msg)
{
  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const rosidl_typesupport_introspection_c__MessageMember * member = members->members_ + i;
    char * ros_message_field = (char *)(ros_message) + member->offset_;
    switch (member->type_id_) {
    }
  }
}

size_t get_serialized_size(
  const void * ros_message,
  const rosidl_typesupport_introspection_c__MessageMembers * members)
{
  assert(members);
  assert(ros_message);
  size_t size = 0;

  for (uint32_t i = 0; i < members->member_count_; ++i) {
    const rosidl_typesupport_introspection_c__MessageMember * member = members->members_ + i;
    const char * ros_message_field = (const char *)(ros_message) + member->offset_;
    const rosidl_runtime_c__String * string = (const rosidl_runtime_c__String *)(ros_message_field);
    switch (member->type_id_) {
      case rosidl_typesupport_introspection_c__ROS_TYPE_MESSAGE:
        {
          // Iterate recursively over the complex ROS messages
          const rosidl_typesupport_introspection_c__MessageMembers * sub_members =
            (const rosidl_typesupport_introspection_c__MessageMembers *)(member->members_
            ->data);

          const void * subros_message = NULL;
          size_t sequence_size = 0;
          size_t sub_members_size = sub_members->size_of_;
          // It's a single message
          if (!member->is_array_) {
            subros_message = ros_message_field;
            sequence_size = 1;
            // It's a fixed size array of messages
          } else if (member->array_size_ > 0 && !member->is_upper_bound_) {
            subros_message = ros_message_field;
            sequence_size = member->array_size_;
            // It's a dynamic sequence of messages
          } else {
            // Cast current ros_message_field ptr as vector "definition"
            const rosidl_runtime_c__char__Sequence * vector =
              (const rosidl_runtime_c__char__Sequence *)(ros_message_field);
            // Vector size points to content of vector and returns number of bytes
            // submembersize is the size of one element in the vector
            // (it is provided by type support)
            sequence_size = vector->size / sub_members_size;
            if (member->is_upper_bound_ && sequence_size > member->array_size_) {
              RMW_SET_ERROR_MSG("vector overcomes the maximum length");
              return RMW_RET_ERROR;
            }
            // create ptr to content of vector to enable recursion
            subros_message = (const void *)(vector->data);
            const uint32_t check = 101;
            uint32_t seq_size[2] = {check, sequence_size};

            size += 2 * sizeof(uint32_t);
          }

          //debug_log("serializing message field %s\n", member->name_);
          for (int index = 0u; index < sequence_size; ++index) {
            size += get_serialized_size(subros_message, sub_members);
          }
        }
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_BOOL:
      case rosidl_typesupport_introspection_c__ROS_TYPE_BYTE:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT8:
      case rosidl_typesupport_introspection_c__ROS_TYPE_CHAR:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT8:
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_FLOAT64:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT16:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT16:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT32:
      case rosidl_typesupport_introspection_c__ROS_TYPE_INT64:
      case rosidl_typesupport_introspection_c__ROS_TYPE_UINT64:
        size += member->size_function(ros_message_field);
        break;
      case rosidl_typesupport_introspection_c__ROS_TYPE_STRING:
        size += string->size;
        break;
      default:
        RMW_SET_ERROR_MSG("Serializing unknown type");
        return RMW_RET_INVALID_ARGUMENT;
    }
  }
}

// rmw_ret_t
// rmw_serialize(
//   const void * ros_message,
//   const rosidl_message_type_support_t * type_supports,
//   rmw_serialized_message_t * serialized_message)
// {
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_INVALID_ARGUMENT);
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);
//   rmw_ret_t ret;

//   if (type_supports->typesupport_identifier != rosidl_typesupport_c__typesupport_identifier) {
//     RMW_SET_ERROR_MSG("rmw_hazcat only provides typesupport for rosidl c");
//     return RMW_RET_INVALID_ARGUMENT;
//   }

//   rosidl_typesupport_introspection_c__MessageMembers * members =
//     (rosidl_typesupport_introspection_c__MessageMembers *)type_supports->data;

//   size_t size = get_serialized_size(ros_message, members);
//   if (ret = rmw_serialized_message_resize(serialized_message, size)
//     != RMW_RET_OK)
//   {
//     RMW_SET_ERROR_MSG("Cannot resize serialized message");
//     return ret;
//   }

//   // TODO: This is a bunch of pseudocode referencing nonexistant functions, fill in gaps
//   if (is_fixed_size(members)) {
//     memcpy(serialized_message, ros_message, size);
//     return RMW_RET_OK;
//   }

//   serialize(serialized_message, ros_message, type_supports);

//   return RMW_RET_OK;
// }

// rmw_ret_t
// rmw_deserialize(
//   const rmw_serialized_message_t * serialized_message,
//   const rosidl_message_type_support_t * type_supports,
//   void * ros_message)
// {
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(serialized_message, RMW_RET_INVALID_ARGUMENT);
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_INVALID_ARGUMENT);
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(ros_message, RMW_RET_INVALID_ARGUMENT);

//   RMW_SET_ERROR_MSG("rmw_deserialize hasn't been implemented yet");
//   return RMW_RET_UNSUPPORTED;
// }

// rmw_ret_t
// rmw_get_serialized_message_size(
//   const rosidl_message_type_support_t * type_supports,
//   const rosidl_runtime_c__Sequence__bound * message_bounds,
//   size_t * size)
// {
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(type_supports, RMW_RET_INVALID_ARGUMENT);
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(message_bounds, RMW_RET_INVALID_ARGUMENT);
//   RCUTILS_CHECK_ARGUMENT_FOR_NULL(size, RMW_RET_INVALID_ARGUMENT);

//   // rosidl_message_type_support_t * ts_c = get_message_typesupport_handle(
//   //   type_supports, rosidl_typesupport_introspection_c__identifier);

//   // const rosidl_typesupport_introspection_c__MessageMembers * members = 
//   //   (const rosidl_typesupport_introspection_c__MessageMembers *)ts_c->data;

//   // // Is this always gonna work even for messages of unfixed sizes?
//   // return members->size_of_;
//   return RMW_RET_UNSUPPORTED;
// }

rmw_ret_t
rmw_serialize(
  const void * ros_message,
  const rosidl_message_type_support_t * type_support,
  rmw_serialized_message_t * serialized_message)
{
  (void)ros_message;
  (void)type_support;
  (void)serialized_message;

  RMW_SET_ERROR_MSG("Function not implemented");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_deserialize(
  const rmw_serialized_message_t * serialized_message,
  const rosidl_message_type_support_t * type_support,
  void * ros_message)
{
  (void)serialized_message;
  (void)type_support;
  (void)ros_message;

  RMW_SET_ERROR_MSG("Function not implemented");
  return RMW_RET_UNSUPPORTED;
}

rmw_ret_t
rmw_get_serialized_message_size(
  const rosidl_message_type_support_t * type_support,
  const rosidl_runtime_c__Sequence__bound * message_bounds,
  size_t * size)
{
  (void)type_support;
  (void)message_bounds;
  (void)size;

  RMW_SET_ERROR_MSG("Function not implemented");
  return RMW_RET_UNSUPPORTED;
}
#ifdef __cplusplus
}
#endif
