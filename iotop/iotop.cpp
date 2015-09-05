// Copyright (C) 2015 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <getopt.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include <algorithm>
#include <map>
#include <unordered_map>
#include <vector>

#include <base/logging.h>

#include "tasklist.h"
#include "taskstats.h"

constexpr uint64_t NSEC_PER_SEC = 1000000000;

static uint64_t BytesToKB(uint64_t bytes) {
  return (bytes + 1024-1) / 1024;
}

static float TimeToTgidPercent(uint64_t ns, int time, const TaskStatistics& stats) {
  float percent = ns / stats.threads() / (time * NSEC_PER_SEC / 100.0f);
  return std::min(percent, 99.99f);
}

static void usage(char* myname) {
  printf(
      "Usage: %s [-h] [-P] [-d <delay>] [-n <cycles>] [-s <column>]\n"
      "   -h  Display this help screen.\n"
      "   -d  Set the delay between refreshes in seconds.\n"
      "   -n  Set the number of refreshes before exiting.\n"
      "   -P  Show processes instead of the default threads.\n"
      "   -s  Set the column to sort by:\n"
      "       pid, read, write, total, io, swap, sched, mem or delay.\n",
      myname);
}

using Sorter = std::function<void(std::vector<TaskStatistics>&)>;
static Sorter GetSorter(const std::string field) {
  auto make_sorter = [](auto field, bool ascending) {
    // Specialized comparator on field and ascending
    auto comparator = [field, ascending](auto& lhs, auto& rhs) -> bool {
      auto a = (lhs.*field)();
      auto b = (rhs.*field)();
      if (a != b) {
        // Sort by selected field
        return ascending ^ (a < b);
      } else {
        // And then fall back to sorting by pid
        return lhs.pid() < rhs.pid();
      }
    };

    // Return closure to std::sort with specialized comparator
    return [comparator](auto& vector) {
      return std::sort(vector.begin(), vector.end(), comparator);
    };
  };

  static const std::map<std::string, Sorter> sorters{
      {"pid", make_sorter(&TaskStatistics::pid, false)},
      {"read", make_sorter(&TaskStatistics::read, true)},
      {"write", make_sorter(&TaskStatistics::write, true)},
      {"total", make_sorter(&TaskStatistics::read_write, true)},
      {"io", make_sorter(&TaskStatistics::delay_io, true)},
      {"swap", make_sorter(&TaskStatistics::delay_swap, true)},
      {"sched", make_sorter(&TaskStatistics::delay_sched, true)},
      {"mem", make_sorter(&TaskStatistics::delay_mem, true)},
      {"delay", make_sorter(&TaskStatistics::delay_total, true)},
  };

  auto it = sorters.find(field);
  if (it == sorters.end()) {
    return nullptr;
  }
  return it->second;
}

int main(int argc, char* argv[]) {
  bool processes = false;
  int delay = 1;
  int cycles = -1;
  int limit = -1;
  Sorter sorter = GetSorter("total");

  android::base::InitLogging(argv, android::base::StderrLogger);

  while (1) {
    int c;
    static const option longopts[] = {
        {"delay", required_argument, 0, 'd'},
        {"help", 0, 0, 'h'},
        {"limit", required_argument, 0, 'm'},
        {"iter", required_argument, 0, 'n'},
        {"sort", required_argument, 0, 's'},
        {"processes", 0, 0, 'P'},
        {0, 0, 0, 0},
    };
    c = getopt_long(argc, argv, "d:hm:n:s:P", longopts, NULL);
    if (c < 0) {
      break;
    }
    switch (c) {
    case 'd':
      delay = atoi(optarg);
      break;
    case 'h':
      usage(argv[0]);
      return(EXIT_SUCCESS);
    case 'm':
      limit = atoi(optarg);
      break;
    case 'n':
      cycles = atoi(optarg);
      break;
    case 's': {
      sorter = GetSorter(optarg);
      if (sorter == nullptr) {
        LOG(ERROR) << "Invalid sort column \"" << optarg << "\"";
        usage(argv[0]);
        return(EXIT_FAILURE);
      }
      break;
    }
    case 'P':
      processes = true;
      break;
    case '?':
      usage(argv[0]);
      return(EXIT_FAILURE);
    default:
      abort();
    }
  }

  std::map<pid_t, std::vector<pid_t>> tgid_map;

  TaskstatsSocket taskstats_socket;
  taskstats_socket.Open();

  std::unordered_map<pid_t, TaskStatistics> pid_stats;
  std::unordered_map<pid_t, TaskStatistics> tgid_stats;
  std::vector<TaskStatistics> stats;

  bool first = true;
  bool second = true;

  while (true) {
    stats.clear();
    if (!TaskList::Scan(tgid_map)) {
      LOG(FATAL) << "failed to scan tasks";
    }
    for (auto& tgid_it : tgid_map) {
      pid_t tgid = tgid_it.first;
      std::vector<pid_t>& pid_list = tgid_it.second;

      TaskStatistics tgid_stats_new;
      TaskStatistics tgid_stats_delta;

      if (processes) {
        // If printing processes, collect stats for the tgid which will
        // hold delay accounting data across all threads, including
        // ones that have exited.
        if (!taskstats_socket.GetTgidStats(tgid, tgid_stats_new)) {
          continue;
        }
        tgid_stats_delta = tgid_stats[tgid].Update(tgid_stats_new);
      }

      // Collect per-thread stats
      for (pid_t pid : pid_list) {
        TaskStatistics pid_stats_new;
        if (!taskstats_socket.GetPidStats(pid, pid_stats_new)) {
          continue;
        }

        TaskStatistics pid_stats_delta = pid_stats[pid].Update(pid_stats_new);

        if (processes) {
          tgid_stats_delta.AddPidToTgid(pid_stats_delta);
        } else {
          stats.push_back(pid_stats_delta);
        }
      }

      if (processes) {
        stats.push_back(tgid_stats_delta);
      }
    }

    if (!first) {
      sorter(stats);
      if (!second) {
        printf("\n");
      }
      printf("%6s %-16s %20s %34s\n", "", "",
          "--- IO (KiB/s) ---", "----------- delayed on ----------");
      printf("%6s %-16s %6s %6s %6s  %-5s  %-5s  %-5s  %-5s  %-5s\n",
          "PID",
          "Command",
          "read",
          "write",
          "total",
          "IO",
          "swap",
          "sched",
          "mem",
          "total");
      int n = limit;
      for (const TaskStatistics& statistics : stats) {
        printf("%6d %-16s %6" PRIu64 " %6" PRIu64 " %6" PRIu64 " %5.2f%% %5.2f%% %5.2f%% %5.2f%% %5.2f%%\n",
            statistics.pid(),
            statistics.comm().c_str(),
            BytesToKB(statistics.read()),
            BytesToKB(statistics.write()),
            BytesToKB(statistics.read_write()),
            TimeToTgidPercent(statistics.delay_io(), delay, statistics),
            TimeToTgidPercent(statistics.delay_swap(), delay, statistics),
            TimeToTgidPercent(statistics.delay_sched(), delay, statistics),
            TimeToTgidPercent(statistics.delay_mem(), delay, statistics),
            TimeToTgidPercent(statistics.delay_total(), delay, statistics));
        if (n > 0 && --n == 0) break;
      }
      second = false;

      if (cycles > 0 && --cycles == 0) break;
    }
    first = false;
    sleep(delay);
  }

  return 0;
}
