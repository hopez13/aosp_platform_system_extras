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

#ifndef SIMPLE_PERF_SAMPLE_DISPLAYER_H_
#define SIMPLE_PERF_SAMPLE_DISPLAYER_H_

#include <inttypes.h>

#include <string>

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

// The display functions below are used to show items in a sample.

template <typename EntryT, typename InfoT>
std::string DisplayAccumulatedOverhead(const EntryT* sample,
                                       const InfoT* info) {
  uint64_t period = sample->period + sample->accumulated_period;
  uint64_t total_period = info->total_period;
  double percentage = (total_period != 0) ? 100.0 * period / total_period : 0.0;
  return android::base::StringPrintf("%.2f%%", percentage);
}

template <typename EntryT, typename InfoT>
std::string DisplaySelfOverhead(const EntryT* sample, const InfoT* info) {
  uint64_t period = sample->period;
  uint64_t total_period = info->total_period;
  double percentage = (total_period != 0) ? 100.0 * period / total_period : 0.0;
  return android::base::StringPrintf("%.2f%%", percentage);
}

#define BUILD_DISPLAY_UINT64_FUNCTION(function_name, display_part)        \
  template <typename EntryT>                                              \
  std::string function_name(const EntryT* sample) {                       \
    return android::base::StringPrintf("%" PRIu64, sample->display_part); \
  }

#define BUILD_DISPLAY_HEX64_FUNCTION(function_name, display_part)         \
  template <typename EntryT>                                              \
  std::string function_name(const EntryT* sample) {                       \
    return android::base::StringPrintf("0x%" PRIx64, sample->display_part);\
}

BUILD_DISPLAY_UINT64_FUNCTION(DisplaySampleCount, sample_count);

template <typename EntryT>
std::string DisplayPid(const EntryT* sample) {
  return android::base::StringPrintf("%d", sample->thread->pid);
}

template <typename EntryT>
std::string DisplayTid(const EntryT* sample) {
  return android::base::StringPrintf("%d", sample->thread->tid);
}

template <typename EntryT>
std::string DisplayComm(const EntryT* sample) {
  return sample->thread_comm;
}

template <typename EntryT>
std::string DisplayDso(const EntryT* sample) {
  return sample->map->dso->Path();
}

template <typename EntryT>
std::string DisplaySymbol(const EntryT* sample) {
  return sample->symbol->DemangledName();
}

template <typename EntryT>
std::string DisplayDsoFrom(const EntryT* sample) {
  return sample->branch_from.map->dso->Path();
}

template <typename EntryT>
std::string DisplaySymbolFrom(const EntryT* sample) {
  return sample->branch_from.symbol->DemangledName();
}

template <typename CallChainNodeT>
void DisplayCallGraphEntry(FILE* fp, size_t depth, std::string prefix,
                           const std::unique_ptr<CallChainNodeT>& node,
                           uint64_t parent_period, bool last) {
  if (depth > 20) {
    LOG(WARNING) << "truncated callgraph at depth " << depth;
    return;
  }
  prefix += "|";
  fprintf(fp, "%s\n", prefix.c_str());
  if (last) {
    prefix.back() = ' ';
  }
  std::string percentage_s = "-- ";
  if (node->period + node->children_period != parent_period) {
    double percentage =
        100.0 * (node->period + node->children_period) / parent_period;
    percentage_s = android::base::StringPrintf("--%.2f%%-- ", percentage);
  }
  fprintf(fp, "%s%s%s\n", prefix.c_str(), percentage_s.c_str(),
          node->chain[0]->symbol->DemangledName());
  prefix.append(percentage_s.size(), ' ');
  for (size_t i = 1; i < node->chain.size(); ++i) {
    fprintf(fp, "%s%s\n", prefix.c_str(),
            node->chain[i]->symbol->DemangledName());
  }
  for (size_t i = 0; i < node->children.size(); ++i) {
    DisplayCallGraphEntry(fp, depth + 1, prefix, node->children[i],
                          node->children_period,
                          (i + 1 == node->children.size()));
  }
}

template <typename EntryT>
void DisplayCallgraph(FILE* fp, const EntryT* sample) {
  std::string prefix = "       ";
  fprintf(fp, "%s|\n", prefix.c_str());
  fprintf(fp, "%s-- %s\n", prefix.c_str(), sample->symbol->DemangledName());
  prefix.append(3, ' ');
  for (size_t i = 0; i < sample->callchain.children.size(); ++i) {
    DisplayCallGraphEntry(fp, 1, prefix, sample->callchain.children[i],
                          sample->callchain.children_period,
                          (i + 1 == sample->callchain.children.size()));
  }
}

// SampleDisplayer is a class using a collections of display functions to show a
// sample.

template <typename EntryT, typename InfoT>
class SampleDisplayer {
 public:
  typedef std::string (*display_sample_func_t)(const EntryT*);
  typedef std::string (*display_sample_with_info_func_t)(const EntryT*,
                                                         const InfoT*);
  typedef void (*exclusive_display_sample_func_t)(FILE* fp, const EntryT*);

 private:
  struct Item {
    std::string name;
    size_t width;
    display_sample_func_t func;
    display_sample_with_info_func_t func_with_info;
  };

 public:
  void SetInfo(const InfoT* info) { info_ = info; }

  void AddDisplayFunction(const std::string& name, display_sample_func_t func) {
    Item item;
    item.name = name;
    item.width = name.size();
    item.func = func;
    item.func_with_info = nullptr;
    display_v_.push_back(item);
  }

  void AddDisplayFunction(const std::string& name,
                          display_sample_with_info_func_t func_with_info) {
    Item item;
    item.name = name;
    item.width = name.size();
    item.func = nullptr;
    item.func_with_info = func_with_info;
    display_v_.push_back(item);
  }

  void AddExclusiveDisplayFunction(exclusive_display_sample_func_t func) {
    exclusive_display_v_.push_back(func);
  }

  void AdjustWidth(const EntryT* sample) {
    for (auto& item : display_v_) {
      std::string data = (item.func != nullptr)
                             ? item.func(sample)
                             : item.func_with_info(sample, info_);
      item.width = std::max(item.width, data.size());
    }
  }

  void PrintNames(FILE* fp) {
    for (size_t i = 0; i < display_v_.size(); ++i) {
      auto& item = display_v_[i];
      if (i != display_v_.size() - 1) {
        fprintf(fp, "%-*s  ", static_cast<int>(item.width), item.name.c_str());
      } else {
        fprintf(fp, "%s\n", item.name.c_str());
      }
    }
  }

  void PrintSample(FILE* fp, const EntryT* sample) {
    for (size_t i = 0; i < display_v_.size(); ++i) {
      auto& item = display_v_[i];
      std::string data = (item.func != nullptr)
                             ? item.func(sample)
                             : item.func_with_info(sample, info_);
      if (i != display_v_.size() - 1) {
        fprintf(fp, "%-*s  ", static_cast<int>(item.width), data.c_str());
      } else {
        fprintf(fp, "%s\n", data.c_str());
      }
    }
    for (auto& func : exclusive_display_v_) {
      func(fp, sample);
    }
  }

 private:
  const InfoT* info_;
  std::vector<Item> display_v_;
  std::vector<exclusive_display_sample_func_t> exclusive_display_v_;
};

#endif  // SIMPLE_PERF_SAMPLE_DISPLAYER_H_
