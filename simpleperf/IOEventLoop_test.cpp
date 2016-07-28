/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "IOEventLoop.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

TEST(IOEventLoop, read) {
  int fd[2];
  ASSERT_EQ(0, pipe(fd));
  IOEventLoop loop;
  static int count;
  static int retry_count;
  count = 0;
  retry_count = 0;
  ASSERT_TRUE(loop.AddReadEvent(fd[0], [&]() {
    while (true) {
      char c;
      int ret = read(fd[0], &c, 1);
      if (ret == 1) {
        if (++count == 100) {
          return loop.ExitLoop();
        }
      } else if (ret == -1 && errno == EAGAIN) {
        retry_count++;
        break;
      } else {
        return false;
      }
    }
    return true;
  }));
  std::thread thread([&]() {
    for (int i = 0; i < 100; ++i) {
      usleep(1000);
      char c;
      write(fd[1], &c, 1);
    }
  });
  ASSERT_TRUE(loop.RunLoop());
  thread.join();
  ASSERT_EQ(100, count);
  // Test retry_count to make sure we are not doing blocking read.
  ASSERT_GT(retry_count, 0);
  close(fd[0]);
  close(fd[1]);
}

TEST(IOEventLoop, signal) {
  IOEventLoop loop;
  static int count;
  count = 0;
  ASSERT_TRUE(loop.AddSignalEvent(SIGINT, [&]() {
    if (++count == 100) {
      loop.ExitLoop();
    }
    return true;
  }));
  std::thread thread([]() {
    for (int i = 0; i < 100; ++i) {
      usleep(1000);
      kill(getpid(), SIGINT);
    }
  });
  ASSERT_TRUE(loop.RunLoop());
  thread.join();
  ASSERT_EQ(100, count);
}

TEST(IOEventLoop, periodic) {
  timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 1000;
  static int count;
  count = 0;
  IOEventLoop loop;
  ASSERT_TRUE(loop.AddPeriodicEvent(tv, [&]() {
    if (++count == 100) {
      loop.ExitLoop();
    }
    return true;
  }));
  auto start_time = std::chrono::steady_clock::now();
  ASSERT_TRUE(loop.RunLoop());
  auto end_time = std::chrono::steady_clock::now();
  ASSERT_EQ(100, count);
  double time_used = std::chrono::duration_cast<std::chrono::duration<double>>(
                         end_time - start_time)
                         .count();
  // time_used is 0.1 if running precisely, and we accept small errors by using
  // a range [0.1, 0.15).
  ASSERT_GE(time_used, 0.1);
  ASSERT_LT(time_used, 0.15);
}
