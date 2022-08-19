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

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "osrf_testing_tools_cpp/scope_exit.hpp"

#ifdef __linux__
#include <sys/epoll.h>
#endif

#include "rcutils/allocator.h"
#include "rcutils/strdup.h"
#include "rcutils/testing/fault_injection.h"

#include "rmw/error_handling.h"
#include "rmw/rmw.h"

#include "rmw_hazcat/types.h"

using std::atomic;
using std::atomic_int;
using std::atomic_uint_fast32_t;

class TestGuardCondition : public ::testing::Test
{
protected:
  void SetUp() override
  {
    rmw_init_options_t options = rmw_get_zero_initialized_init_options();
    rmw_ret_t ret = rmw_init_options_init(&options, rcutils_get_default_allocator());
    ASSERT_EQ(RMW_RET_OK, ret) << rcutils_get_error_string().str;
    OSRF_TESTING_TOOLS_CPP_SCOPE_EXIT(
    {
      rmw_ret_t ret = rmw_init_options_fini(&options);
      EXPECT_EQ(RMW_RET_OK, ret) << rmw_get_error_string().str;
    });
    options.enclave = rcutils_strdup("/", rcutils_get_default_allocator());
    ASSERT_STREQ("/", options.enclave);
    context = rmw_get_zero_initialized_context();
    ret = rmw_init(&options, &context);
    ASSERT_EQ(RMW_RET_OK, ret) << rcutils_get_error_string().str;
  }

  void TearDown() override
  {
    rmw_ret_t ret = rmw_shutdown(&context);
    EXPECT_EQ(RMW_RET_OK, ret) << rcutils_get_error_string().str;
    ret = rmw_context_fini(&context);
    EXPECT_EQ(RMW_RET_OK, ret) << rcutils_get_error_string().str;
  }

  rmw_context_t context;
};

TEST_F(TestGuardCondition, signal_test) {
  rmw_guard_condition_t * gc1 = rmw_create_guard_condition(&context);
  rmw_guard_condition_t * gc2 = rmw_create_guard_condition(&context);
  guard_condition_t * gc1_impl = reinterpret_cast<guard_condition_t *>(gc1->data);
  guard_condition_t * gc2_impl = reinterpret_cast<guard_condition_t *>(gc2->data);

  bool check = false;

  // Set check to true and trigger guard
  check = true;
  ASSERT_EQ(rmw_trigger_guard_condition(gc1), RMW_RET_OK);

  std::thread t([&]() {
      // Wait on gc, other thread should set check true and then trigger guard
      int epollfd = epoll_create1(0);
      struct epoll_event ev;
      if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
      }
      ASSERT_NE(epoll_ctl(epollfd, EPOLL_CTL_ADD, gc1_impl->pfd[0], &gc1_impl->ev), -1);
      ASSERT_NE(epoll_wait(epollfd, &ev, 1, 10), -1);
      close(epollfd);

      EXPECT_TRUE(check);

      // Set check back to false and trigger guard
      check = false;
      ASSERT_EQ(rmw_trigger_guard_condition(gc2), RMW_RET_OK);
      close(epollfd);
    });

  // Wait on gc, other thread should set check to false and trigger guard
  struct epoll_event ev;
  int epollfd = epoll_create1(0);
  ASSERT_NE(epollfd, -1);
  ASSERT_NE(epoll_ctl(epollfd, EPOLL_CTL_ADD, gc2_impl->pfd[0], &gc2_impl->ev), -1);
  ASSERT_NE(epoll_wait(epollfd, &ev, 1, 10), -1);
  close(epollfd);

  EXPECT_FALSE(check);

  rmw_destroy_guard_condition(gc1);
  rmw_destroy_guard_condition(gc2);
  t.join();
}
