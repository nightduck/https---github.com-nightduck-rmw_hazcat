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
  set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall --std=c++11 -O3 -fPIC")
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(CUDA REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(rcutils REQUIRED)
find_package(rmw REQUIRED)
find_package(rosidl_runtime_c REQUIRED)
find_package(rosidl_typesupport_introspection_c REQUIRED)
find_package(microcdr REQUIRED)

set(CUDA_SEPARABLE_COMPILATION ON)
cuda_select_nvcc_arch_flags(ARCH_FLAGS Auto)
set(CUDA_NVCC_FLAGS ${ARCH_FLAGS} CACHE STRING "nvcc flags" FORCE)

include_directories(include)

include_directories(${CUDA_INCLUDE_DIRS})

# TODO: Separate out different allocators by domain based on which ones need extra libraries
set(hazcat_allocator_sources
  src/allocators/hma_template.c
  src/allocators/cpu_ringbuf_allocator.c
  src/allocators/cuda_ringbuf_allocator.c
)

set(hazcat_message_queue_sources
  src/hazcat_message_queue.c
  src/guard_condition_impl.c
)

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

add_library(hazcat_allocators ${hazcat_allocator_sources})
target_include_directories(hazcat_allocators PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
  target_link_libraries(hazcat_allocators cuda)
  ament_target_dependencies(hazcat_allocators
    CUDA
    rcutils
  )


add_library(hazcat_message_queue ${hazcat_message_queue_sources})
target_include_directories(hazcat_message_queue PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")
  ament_target_dependencies(hazcat_message_queue
    rmw
  )

add_library(rmw_hazcat SHARED ${rmw_hazcat_sources})
ament_target_dependencies(rmw_hazcat
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
  hazcat_allocators
  hazcat_message_queue
  microcdr
)

register_rmw_implementation(
  "c:rosidl_typesupport_c:rosidl_typesupport_introspection_c"
)
configure_rmw_library(rmw_hazcat)

install(
  TARGETS hazcat_allocators hazcat_message_queue rmw_hazcat EXPORT rmw_hazcat
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

  ament_add_gtest(allocator_cpu_test test/hazcat_cpu_allocator_test.cpp)
  target_link_libraries(allocator_cpu_test hazcat_allocators)
  ament_target_dependencies(allocator_cpu_test
    test_msgs std_msgs
    rcutils
  )

  ament_add_gtest(allocator_cuda_test test/hazcat_cuda_allocator_test.cpp)
  target_link_libraries(allocator_cuda_test hazcat_allocators)
  ament_target_dependencies(allocator_cuda_test
    test_msgs std_msgs
    rcutils
  )

  ament_add_gtest(guard_condition_test test/hazcat_guard_condition.cpp)
  target_link_libraries(guard_condition_test rmw_hazcat)
  ament_target_dependencies(guard_condition_test
    rcutils
  )

  ament_add_gtest(message_queue_test test/hazcat_message_queue_test.cpp)
  target_link_libraries(message_queue_test hazcat_message_queue hazcat_allocators rmw_hazcat)
  ament_target_dependencies(message_queue_test
    test_msgs std_msgs
    rcutils
  )
endif()

ament_package()
