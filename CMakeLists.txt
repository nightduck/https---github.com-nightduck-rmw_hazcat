cmake_minimum_required(VERSION 3.8)
set(CMAKE_CUDA_COMPILER /usr/local/cuda/bin/nvcc)
project(rmw_hazcat CUDA CXX C)

# Default to C11
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 11)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(CUDA REQUIRED)
find_package(ament_cmake REQUIRED)
find_package(rosidl_runtime_c REQUIRED)

SET(CUDA_SEPARABLE_COMPILATION ON)
CUDA_SELECT_NVCC_ARCH_FLAGS(ARCH_FLAGS Auto)
SET(CUDA_NVCC_FLAGS ${ARCH_FLAGS} CACHE STRING "nvcc flags" FORCE)

include_directories(include)

include_directories(${CUDA_INCLUDE_DIRS})

set(${PROJECT_NAME}_sources
  src/allocators/hma_template.cpp
  )

add_library(allocators ${${PROJECT_NAME}_sources})
target_include_directories(allocators PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
  "$<INSTALL_INTERFACE:include>")

install(
  TARGETS allocators EXPORT ${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

ament_export_include_directories(include)
ament_export_libraries(allocators)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)

  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)
  find_package(test_msgs REQUIRED)

  ament_add_gtest(allocator_test test/hazcat_allocator_test.cpp)
  target_link_libraries(allocator_test allocators cuda)
  ament_target_dependencies(allocator_test
    CUDA test_msgs
  )
endif()

ament_package()
