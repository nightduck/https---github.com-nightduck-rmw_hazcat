cmake_minimum_required(VERSION 3.8)
set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)
project(rmw_hazcat CUDA CXX C)

# Default to C11
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wall -O3 -fPIC")
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall --std=c++11 -O3 -fPIC -fno-omit-frame-pointer")
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(CUDA REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(rcutils REQUIRED)
find_package(hazcat REQUIRED)
find_package(hazcat_allocators REQUIRED)
find_package(rmw REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_introspection_c REQUIRED)
find_package(rosidl_typesupport_introspection_cpp REQUIRED)
find_package(microcdr REQUIRED)

set(CUDA_SEPARABLE_COMPILATION ON)
cuda_select_nvcc_arch_flags(ARCH_FLAGS Auto)
set(CUDA_NVCC_FLAGS ${ARCH_FLAGS} CACHE STRING "nvcc flags" FORCE)

include_directories(include)

include_directories(${CUDA_INCLUDE_DIRS})

set(rmw_hazcat_sources
  src/rmw_client.c
  src/rmw_compare_guids_equal.c
  src/rmw_count.c
  src/rmw_event.c
  src/rmw_get_implementation_identifier.c
  src/rmw_get_serialization_format.c
  src/rmw_guard_condition.c
  src/rmw_init.c
  src/rmw_logging.c
  src/rmw_node_info_and_types.c
  src/rmw_node.c
  src/rmw_publisher.c
  src/rmw_serialize.c
  src/rmw_service.c
  src/rmw_subscription.c
  src/rmw_wait.c
)

set(hazcat_typesupport_sources
  src/hazcat_typesupport.cpp
)

add_library(hazcat_typesupport ${hazcat_typesupport_sources})
ament_target_dependencies(hazcat_typesupport
  rcutils
  rosidl_runtime_c
  rosidl_typesupport_introspection_c
  rosidl_typesupport_introspection_cpp
)

add_library(rmw_hazcat SHARED ${rmw_hazcat_sources})
ament_target_dependencies(rmw_hazcat
  hazcat
  hazcat_allocators
  microcdr
  rcutils
  rmw
  rosidl_runtime_c
  rosidl_typesupport_introspection_c
)
target_include_directories(
  rmw_hazcat
  PUBLIC include
)
target_link_libraries(rmw_hazcat
  microcdr
  hazcat_typesupport
)

register_rmw_implementation(
  "c:rosidl_typesupport_c:rosidl_typesupport_introspection_c"
)
configure_rmw_library(rmw_hazcat)

install(
  TARGETS rmw_hazcat hazcat_typesupport
  EXPORT rmw_hazcat
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

ament_export_include_directories(include)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)

  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  find_package(test_msgs REQUIRED)
  find_package(std_msgs REQUIRED)

  ament_add_gtest(guard_condition_test test/hazcat_guard_condition.cpp)
  ament_target_dependencies(guard_condition_test
    rcutils
    hazcat
    hazcat_allocators
  )
  target_link_libraries(guard_condition_test rmw_hazcat)

  ament_add_gtest(message_queue_test test/hazcat_message_queue_test.cpp)
  ament_target_dependencies(message_queue_test
    test_msgs std_msgs
    rcutils
    hazcat
    hazcat_allocators
  )
  target_link_libraries(message_queue_test rmw_hazcat)
endif()

ament_package()
