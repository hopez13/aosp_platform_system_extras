/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef SIMPLE_PERF_SAMPLE_TREE_H_
#define SIMPLE_PERF_SAMPLE_TREE_H_

#include <functional>
#include <set>
#include <string>
#include <unordered_map>

struct ProcessEntry {
  int pid;
  std::string comm;
};

struct MapEntry {
  int pid;  // pid = -1 for kernel map entries.
  uint64_t start_addr;
  uint64_t len;
  uint64_t pgoff;
  uint64_t time;  // Map creation time.
  std::string filename;
};

struct SampleEntry {
  int tid;
  uint64_t ip;
  uint64_t time;
  uint64_t period;
  uint64_t sample_count;
  const ProcessEntry* process_entry;
  const MapEntry* map_entry;
};

typedef std::function<int (const SampleEntry&, const SampleEntry&)> compare_sample_func_t;

class SampleTree {
 public:
  SampleTree(compare_sample_func_t sample_compare_function)
      : sample_comparator_(sample_compare_function),
        sample_tree_(sample_comparator_),
        sorted_sample_comparator_(sample_compare_function),
        sorted_sample_tree_(sorted_sample_comparator_),
        total_samples_(0),
        total_period_(0) {
  }


  void AddProcess(int pid, const std::string& comm);
  void AddKernelMap(uint64_t start_addr, uint64_t len, uint64_t pgoff,
                    uint64_t time, const std::string& filename);
  void AddUserMap(int pid, uint64_t start_addr, uint64_t len, uint64_t pgoff,
                     uint64_t time, const std::string& filename);
  void AddSample(int pid, int tid, uint64_t ip, uint64_t time, uint64_t period);
  void VisitAllSamples(std::function<void(const SampleEntry&)> callback);

  uint64_t TotalSamples() const {
    return total_samples_;
  }

  uint64_t TotalPeriod() const {
    return total_period_;
  }

 private:
  const ProcessEntry* FindProcessEntryOrNew(int pid);
  const MapEntry* FindMapEntryOrNew(int pid, uint64_t ip);

  struct MapComparator {
    bool operator()(const MapEntry& map1, const MapEntry& map2);
  };

  struct SampleComparator {
    bool operator()(const SampleEntry& sample1, const SampleEntry& sample2) {
      return compare_function(sample1, sample2) < 0;
    }
    SampleComparator(compare_sample_func_t compare_function) : compare_function(compare_function) {
    }

    compare_sample_func_t compare_function;
  };

  struct SortedSampleComparator {
    bool operator()(const SampleEntry& sample1, const SampleEntry& sample2) {
      if (sample1.period != sample2.period) {
        return sample1.period > sample2.period;
      }
      return compare_function(sample1, sample2) < 0;
    }
    SortedSampleComparator(compare_sample_func_t compare_function) : compare_function(compare_function) {
    }

    compare_sample_func_t compare_function;
  };

  std::unordered_map<int, ProcessEntry> process_tree_;

  std::set<MapEntry, MapComparator> kernel_map_tree_;
  std::set<MapEntry, MapComparator> user_map_tree_;
  std::unordered_map<int, MapEntry> unknown_maps_;

  SampleComparator sample_comparator_;
  std::set<SampleEntry, SampleComparator> sample_tree_;
  SortedSampleComparator sorted_sample_comparator_;
  std::set<SampleEntry, SortedSampleComparator> sorted_sample_tree_;

  uint64_t total_samples_;
  uint64_t total_period_;
};

#endif  // SIMPLE_PERF_SAMPLE_TREE_H_
