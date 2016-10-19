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

#include "event_selection_set.h"

#include <android-base/logging.h>

#include "environment.h"
#include "event_attr.h"
#include "event_type.h"
#include "IOEventLoop.h"
#include "perf_regs.h"
#include "utils.h"

constexpr uint64_t DEFAULT_SAMPLE_FREQ_FOR_NONTRACEPOINT_EVENT = 4000;
constexpr uint64_t DEFAULT_SAMPLE_PERIOD_FOR_TRACEPOINT_EVENT = 1;

bool IsBranchSamplingSupported() {
  const EventType* type = FindEventTypeByName("cpu-cycles");
  if (type == nullptr) {
    return false;
  }
  perf_event_attr attr = CreateDefaultPerfEventAttr(*type);
  attr.sample_type |= PERF_SAMPLE_BRANCH_STACK;
  attr.branch_sample_type = PERF_SAMPLE_BRANCH_ANY;
  return IsEventAttrSupportedByKernel(attr);
}

bool IsDwarfCallChainSamplingSupported() {
  const EventType* type = FindEventTypeByName("cpu-cycles");
  if (type == nullptr) {
    return false;
  }
  perf_event_attr attr = CreateDefaultPerfEventAttr(*type);
  attr.sample_type |=
      PERF_SAMPLE_CALLCHAIN | PERF_SAMPLE_REGS_USER | PERF_SAMPLE_STACK_USER;
  attr.exclude_callchain_user = 1;
  attr.sample_regs_user = GetSupportedRegMask(GetBuildArch());
  attr.sample_stack_user = 8192;
  return IsEventAttrSupportedByKernel(attr);
}

bool EventSelectionSet::BuildAndCheckEventSelection(
    const std::string& event_name, EventSelection* selection) {
  std::unique_ptr<EventTypeAndModifier> event_type = ParseEventType(event_name);
  if (event_type == nullptr) {
    return false;
  }
  if (for_stat_cmd_) {
    if (event_type->event_type.name == "cpu-clock" ||
        event_type->event_type.name == "task-clock") {
      if (event_type->exclude_user || event_type->exclude_kernel) {
        LOG(ERROR) << "Modifier u and modifier k used in event type "
                   << event_type->event_type.name
                   << " are not supported by the kernel.";
        return false;
      }
    }
  }
  selection->event_type_modifier = *event_type;
  selection->event_attr = CreateDefaultPerfEventAttr(event_type->event_type);
  selection->event_attr.exclude_user = event_type->exclude_user;
  selection->event_attr.exclude_kernel = event_type->exclude_kernel;
  selection->event_attr.exclude_hv = event_type->exclude_hv;
  selection->event_attr.exclude_host = event_type->exclude_host;
  selection->event_attr.exclude_guest = event_type->exclude_guest;
  selection->event_attr.precise_ip = event_type->precise_ip;
  if (selection->event_attr.type != USER_SPACE_SAMPLER_EVENT_TYPE &&
      !IsEventAttrSupportedByKernel(selection->event_attr)) {
    LOG(ERROR) << "Event type '" << event_type->name
               << "' is not supported by the kernel";
    return false;
  }
  if (selection->event_attr.type == USER_SPACE_SAMPLER_EVENT_TYPE &&
      selection->event_attr.config == INPLACE_SAMPLER_CONFIG) {
    // Fix event attr to record call chain.
    selection->event_attr.sample_type |= PERF_SAMPLE_CALLCHAIN;
  }
  selection->event_fds.clear();

  for (const auto& group : groups_) {
    for (const auto& sel : group) {
      if (sel.event_type_modifier.name == selection->event_type_modifier.name) {
        LOG(ERROR) << "Event type '" << sel.event_type_modifier.name
                   << "' appears more than once";
        return false;
      }
    }
  }
  return true;
}

bool EventSelectionSet::AddEventType(const std::string& event_name) {
  return AddEventGroup(std::vector<std::string>(1, event_name));
}

bool EventSelectionSet::AddEventGroup(
    const std::vector<std::string>& event_names) {
  EventSelectionGroup group;
  for (const auto& event_name : event_names) {
    EventSelection selection;
    if (!BuildAndCheckEventSelection(event_name, &selection)) {
      return false;
    }
    group.push_back(std::move(selection));
  }
  bool has_user_space_sampler = false;
  for (const auto& selection : group) {
    if (selection.event_attr.type == USER_SPACE_SAMPLER_EVENT_TYPE) {
      has_user_space_sampler = true;
    }
  }
  if (has_user_space_sampler) {
    if (group.size() > 1) {
      LOG(ERROR) << "User space sampler can't be grouped with other events.";
      return false;
    }
    if (for_stat_cmd_) {
      LOG(ERROR) << "User space sampler is not supported on stat command.";
      return false;
    }
  }
  groups_.push_back(std::move(group));
  UnionSampleType();
  return true;
}

std::vector<const EventType*> EventSelectionSet::GetTracepointEvents() const {
  std::vector<const EventType*> result;
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      if (selection.event_type_modifier.event_type.type ==
          PERF_TYPE_TRACEPOINT) {
        result.push_back(&selection.event_type_modifier.event_type);
      }
    }
  }
  return result;
}

std::vector<EventAttrWithId> EventSelectionSet::GetEventAttrWithId() const {
  std::vector<EventAttrWithId> result;
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      EventAttrWithId attr_id;
      attr_id.attr = &selection.event_attr;
      for (const auto& fd : selection.event_fds) {
        attr_id.ids.push_back(fd->Id());
      }
      if (selection.inplace_sampler != nullptr) {
        attr_id.ids.push_back(selection.inplace_sampler->Id());
      }
      result.push_back(attr_id);
    }
  }
  return result;
}

// Union the sample type of different event attrs can make reading sample
// records in perf.data easier.
void EventSelectionSet::UnionSampleType() {
  uint64_t sample_type = 0;
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      sample_type |= selection.event_attr.sample_type;
    }
  }
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_type = sample_type;
    }
  }
}

void EventSelectionSet::SetEnableOnExec(bool enable) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      // If sampling is enabled on exec, then it is disabled at startup,
      // otherwise it should be enabled at startup. Don't use
      // ioctl(PERF_EVENT_IOC_ENABLE) to enable it after perf_event_open().
      // Because some android kernels can't handle ioctl() well when cpu-hotplug
      // happens. See http://b/25193162.
      if (enable) {
        selection.event_attr.enable_on_exec = 1;
        selection.event_attr.disabled = 1;
      } else {
        selection.event_attr.enable_on_exec = 0;
        selection.event_attr.disabled = 0;
      }
    }
  }
}

bool EventSelectionSet::GetEnableOnExec() {
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      if (selection.event_attr.enable_on_exec == 0) {
        return false;
      }
    }
  }
  return true;
}

void EventSelectionSet::SampleIdAll() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_id_all = 1;
    }
  }
}

void EventSelectionSet::SetSampleFreq(uint64_t sample_freq) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.freq = 1;
      selection.event_attr.sample_freq = sample_freq;
    }
  }
}

void EventSelectionSet::SetSamplePeriod(uint64_t sample_period) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.freq = 0;
      selection.event_attr.sample_period = sample_period;
    }
  }
}

void EventSelectionSet::UseDefaultSampleFreq() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      if (selection.event_type_modifier.event_type.type ==
          PERF_TYPE_TRACEPOINT) {
        selection.event_attr.freq = 0;
        selection.event_attr.sample_period =
            DEFAULT_SAMPLE_PERIOD_FOR_TRACEPOINT_EVENT;
      } else {
        selection.event_attr.freq = 1;
        selection.event_attr.sample_freq =
            DEFAULT_SAMPLE_FREQ_FOR_NONTRACEPOINT_EVENT;
      }
    }
  }
}

bool EventSelectionSet::SetBranchSampling(uint64_t branch_sample_type) {
  if (branch_sample_type != 0 &&
      (branch_sample_type &
       (PERF_SAMPLE_BRANCH_ANY | PERF_SAMPLE_BRANCH_ANY_CALL |
        PERF_SAMPLE_BRANCH_ANY_RETURN | PERF_SAMPLE_BRANCH_IND_CALL)) == 0) {
    LOG(ERROR) << "Invalid branch_sample_type: 0x" << std::hex
               << branch_sample_type;
    return false;
  }
  if (branch_sample_type != 0 && !IsBranchSamplingSupported()) {
    LOG(ERROR) << "branch stack sampling is not supported on this device.";
    return false;
  }
  for (auto& group : groups_) {
    for (auto& selection : group) {
      perf_event_attr& attr = selection.event_attr;
      if (branch_sample_type != 0) {
        attr.sample_type |= PERF_SAMPLE_BRANCH_STACK;
      } else {
        attr.sample_type &= ~PERF_SAMPLE_BRANCH_STACK;
      }
      attr.branch_sample_type = branch_sample_type;
    }
  }
  return true;
}

void EventSelectionSet::EnableFpCallChainSampling() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_type |= PERF_SAMPLE_CALLCHAIN;
    }
  }
}

bool EventSelectionSet::EnableDwarfCallChainSampling(uint32_t dump_stack_size) {
  if (!IsDwarfCallChainSamplingSupported()) {
    LOG(ERROR) << "dwarf callchain sampling is not supported on this device.";
    return false;
  }
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.sample_type |= PERF_SAMPLE_CALLCHAIN |
                                          PERF_SAMPLE_REGS_USER |
                                          PERF_SAMPLE_STACK_USER;
      selection.event_attr.exclude_callchain_user = 1;
      selection.event_attr.sample_regs_user =
          GetSupportedRegMask(GetBuildArch());
      selection.event_attr.sample_stack_user = dump_stack_size;
    }
  }
  return true;
}

void EventSelectionSet::SetInherit(bool enable) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.inherit = (enable ? 1 : 0);
    }
  }
}

void EventSelectionSet::SetLowWatermark() {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      selection.event_attr.wakeup_events = 1;
    }
  }
}

bool EventSelectionSet::NeedKernelSymbol() const {
  for (const auto& group : groups_) {
    for (const auto& selection : group) {
      if (!selection.event_type_modifier.exclude_kernel) {
        return true;
      }
    }
  }
  return false;
}

bool EventSelectionSet::IsUserSpaceSamplerGroup(const EventSelectionGroup& group) const {
  return group.size() == 1 && group[0].event_attr.type == USER_SPACE_SAMPLER_EVENT_TYPE;
}

static bool CheckIfCpusOnline(const std::vector<int>& cpus) {
  std::vector<int> online_cpus = GetOnlineCpus();
  for (const auto& cpu : cpus) {
    if (std::find(online_cpus.begin(), online_cpus.end(), cpu) ==
        online_cpus.end()) {
      LOG(ERROR) << "cpu " << cpu << " is not online.";
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::OpenEventFilesOnGroup(EventSelectionGroup& group,
                                              pid_t tid, int cpu,
                                              std::string* failed_event_type) {
  std::vector<std::unique_ptr<EventFd>> event_fds;
  // Given a tid and cpu, events on the same group should be all opened
  // successfully or all failed to open.
  EventFd* group_fd = nullptr;
  for (auto& selection : group) {
    std::unique_ptr<EventFd> event_fd =
        EventFd::OpenEventFile(selection.event_attr, tid, cpu, group_fd);
    if (event_fd != nullptr) {
      LOG(VERBOSE) << "OpenEventFile for " << event_fd->Name();
      event_fds.push_back(std::move(event_fd));
    } else {
      if (failed_event_type != nullptr) {
        *failed_event_type = selection.event_type_modifier.name;
        return false;
      }
    }
    if (group_fd == nullptr) {
      group_fd = event_fd.get();
    }
  }
  for (size_t i = 0; i < group.size(); ++i) {
    group[i].event_fds.push_back(std::move(event_fds[i]));
  }
  return true;
}

bool EventSelectionSet::OpenUserSpaceSampler(EventSelectionGroup& group) {
  CHECK(group.size() == 1);
  if (group[0].event_type_modifier.event_type.config == INPLACE_SAMPLER_CONFIG) {
    group[0].inplace_sampler = InplaceSampler::Create(group[0].event_attr,
                                                      processes_, threads_);
    if (group[0].inplace_sampler != nullptr) {
      return true;
    }
  }
  return false;
}

static std::set<pid_t> PrepareThreads(const std::set<pid_t>& processes,
                                      const std::set<pid_t>& threads) {
  std::set<pid_t> result = threads;
  for (const auto& pid : processes) {
    std::vector<pid_t> tids = GetThreadsInProcess(pid);
    result.insert(tids.begin(), tids.end());
  }
  return result;
}

bool EventSelectionSet::OpenEventFiles(const std::vector<int>& on_cpus) {
  std::vector<int> cpus = on_cpus;
  if (!cpus.empty()) {
    // cpus = {-1} means open an event file for all cpus.
    if (!(cpus.size() == 1 && cpus[0] == -1) && !CheckIfCpusOnline(cpus)) {
      return false;
    }
  } else {
    cpus = GetOnlineCpus();
  }
  std::set<pid_t> threads = PrepareThreads(processes_, threads_);
  for (auto& group : groups_) {
    if (IsUserSpaceSamplerGroup(group)) {
      if (!OpenUserSpaceSampler(group)) {
        return false;
      }
    } else {
      for (const auto& tid : threads) {
        size_t success_cpu_count = 0;
        std::string failed_event_type;
        for (const auto& cpu : cpus) {
          if (OpenEventFilesOnGroup(group, tid, cpu, &failed_event_type)) {
            success_cpu_count++;
          }
        }
        // As the online cpus can be enabled or disabled at runtime, we may not
        // open event file for all cpus successfully. But we should open at
        // least one cpu successfully.
        if (success_cpu_count == 0) {
          PLOG(ERROR) << "failed to open perf event file for event_type "
              << failed_event_type << " for "
              << (tid == -1 ? "all threads"
                  : "thread " + std::to_string(tid))
                    << " on all cpus";
          return false;
        }
      }
    }
  }
  return true;
}

static bool ReadCounter(const EventFd* event_fd, CounterInfo* counter) {
  if (!event_fd->ReadCounter(&counter->counter)) {
    return false;
  }
  counter->tid = event_fd->ThreadId();
  counter->cpu = event_fd->Cpu();
  return true;
}

bool EventSelectionSet::ReadCounters(std::vector<CountersInfo>* counters) {
  counters->clear();
  for (size_t i = 0; i < groups_.size(); ++i) {
    for (auto& selection : groups_[i]) {
      CountersInfo counters_info;
      counters_info.group_id = i;
      counters_info.event_name = selection.event_type_modifier.event_type.name;
      counters_info.event_modifier = selection.event_type_modifier.modifier;
      counters_info.counters = selection.hotplugged_counters;
      for (auto& event_fd : selection.event_fds) {
        CounterInfo counter;
        if (!ReadCounter(event_fd.get(), &counter)) {
          return false;
        }
        counters_info.counters.push_back(counter);
      }
      counters->push_back(counters_info);
    }
  }
  return true;
}

bool EventSelectionSet::MmapEventFiles(size_t min_mmap_pages,
                                       size_t max_mmap_pages) {
  for (size_t i = max_mmap_pages; i >= min_mmap_pages; i >>= 1) {
    if (MmapEventFiles(i, i == min_mmap_pages)) {
      LOG(VERBOSE) << "Mapped buffer size is " << i << " pages.";
      mmap_pages_ = i;
      return true;
    }
    for (auto& group : groups_) {
      for (auto& selection : group) {
        for (auto& event_fd : selection.event_fds) {
          event_fd->DestroyMappedBuffer();
        }
      }
    }
  }
  return false;
}

bool EventSelectionSet::MmapEventFiles(size_t mmap_pages, bool report_error) {
  // Allocate a mapped buffer for each cpu.
  std::map<int, EventFd*> cpu_map;
  for (auto& group : groups_) {
    for (auto& selection : group) {
      for (auto& event_fd : selection.event_fds) {
        auto it = cpu_map.find(event_fd->Cpu());
        if (it != cpu_map.end()) {
          if (!event_fd->ShareMappedBuffer(*(it->second), report_error)) {
            return false;
          }
        } else {
          if (!event_fd->CreateMappedBuffer(mmap_pages, report_error)) {
            return false;
          }
          cpu_map[event_fd->Cpu()] = event_fd.get();
        }
      }
    }
  }
  return true;
}

bool EventSelectionSet::PrepareToReadMmapEventData(
    IOEventLoop& loop, const std::function<bool(Record*)>& callback) {
  // Add read Events for perf event files having mapped buffer.
  for (auto& group : groups_) {
    for (auto& selection : group) {
      for (auto& event_fd : selection.event_fds) {
        if (event_fd->HasMappedBuffer()) {
          if (!event_fd->StartPolling(loop, [&]() {
                return ReadMmapEventDataForFd(event_fd.get());
              })) {
            return false;
          }
        }
      }
      if (selection.inplace_sampler != nullptr) {
        if (!selection.inplace_sampler->StartPolling(loop, callback)) {
          return false;
        }
      }
    }
  }
  loop_ = &loop;

  // Prepare record callback function.
  record_callback_ = callback;
  return true;
}

bool EventSelectionSet::ReadMmapEventDataForFd(EventFd* event_fd) {
  const char* data;
  // Call GetAvailableMmapData() only once instead of calling in a loop, because
  // 1) A mapped buffer caches data before needing to be read again. By default
  //    it raises read Event when half full.
  // 2) Spinning on one mapped buffer can make other mapped buffers overflow.
  size_t size = event_fd->GetAvailableMmapData(&data);
  if (size == 0) {
    return true;
  }
  std::vector<std::unique_ptr<Record>> records =
      ReadRecordsFromBuffer(event_fd->attr(), data, size);
  for (auto& r : records) {
    if (!record_callback_(r.get())) {
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::FinishReadMmapEventData() {
  // Read each mapped buffer once, because some data may exist in the buffers
  // but is not much enough to raise read Events.
  for (auto& group : groups_) {
    for (auto& selection : group) {
      for (auto& event_fd : selection.event_fds) {
        if (event_fd->HasMappedBuffer()) {
          if (!ReadMmapEventDataForFd(event_fd.get())) {
            return false;
          }
        }
      }
    }
  }
  return true;
}

bool EventSelectionSet::HandleCpuHotplugEvents(
    IOEventLoop& loop, const std::vector<int>& monitored_cpus,
    double check_interval_in_sec) {
  monitored_cpus_.insert(monitored_cpus.begin(), monitored_cpus.end());
  online_cpus_ = GetOnlineCpus();
  if (!loop.AddPeriodicEvent(SecondToTimeval(check_interval_in_sec),
                             [&]() { return DetectCpuHotplugEvents(); })) {
    return false;
  }
  return true;
}

bool EventSelectionSet::DetectCpuHotplugEvents() {
  std::vector<int> new_cpus = GetOnlineCpus();
  for (const auto& cpu : online_cpus_) {
    if (std::find(new_cpus.begin(), new_cpus.end(), cpu) == new_cpus.end()) {
      if (monitored_cpus_.empty() ||
          monitored_cpus_.find(cpu) != monitored_cpus_.end()) {
        LOG(INFO) << "Cpu " << cpu << " is offlined";
        if (!HandleCpuOfflineEvent(cpu)) {
          return false;
        }
      }
    }
  }
  for (const auto& cpu : new_cpus) {
    if (std::find(online_cpus_.begin(), online_cpus_.end(), cpu) ==
        online_cpus_.end()) {
      if (monitored_cpus_.empty() ||
          monitored_cpus_.find(cpu) != monitored_cpus_.end()) {
        LOG(INFO) << "Cpu " << cpu << " is onlined";
        if (!HandleCpuOnlineEvent(cpu)) {
          return false;
        }
      }
    }
  }
  online_cpus_ = new_cpus;
  return true;
}

bool EventSelectionSet::HandleCpuOfflineEvent(int cpu) {
  for (auto& group : groups_) {
    for (auto& selection : group) {
      for (auto it = selection.event_fds.begin();
           it != selection.event_fds.end();) {
        if ((*it)->Cpu() == cpu) {
          if (for_stat_cmd_) {
            CounterInfo counter;
            if (!ReadCounter(it->get(), &counter)) {
              return false;
            }
            selection.hotplugged_counters.push_back(counter);
          } else {
            if ((*it)->HasMappedBuffer()) {
              if (!ReadMmapEventDataForFd(it->get())) {
                return false;
              }
              if (!(*it)->StopPolling()) {
                return false;
              }
            }
          }
          it = selection.event_fds.erase(it);
        } else {
          ++it;
        }
      }
    }
  }
  return true;
}

bool EventSelectionSet::HandleCpuOnlineEvent(int cpu) {
  // We need to start profiling when opening new event files.
  SetEnableOnExec(false);
  std::set<pid_t> threads = PrepareThreads(processes_, threads_);
  for (auto& group : groups_) {
    if (IsUserSpaceSamplerGroup(group)) {
      continue;
    }
    for (const auto& tid : threads) {
      std::string failed_event_type;
      if (!OpenEventFilesOnGroup(group, tid, cpu, &failed_event_type)) {
        // If failed to open event files, maybe the cpu has been offlined.
        PLOG(WARNING) << "failed to open perf event file for event_type "
                      << failed_event_type << " for "
                      << (tid == -1 ? "all threads"
                                    : "thread " + std::to_string(tid))
                      << " on cpu " << cpu;
      }
    }
  }
  if (!for_stat_cmd_) {
    // Prepare mapped buffer.
    if (!CreateMappedBufferForCpu(cpu)) {
      return false;
    }
    // Send a EventIdRecord.
    std::vector<uint64_t> event_id_data;
    uint64_t attr_id = 0;
    for (const auto& group : groups_) {
      for (const auto& selection : group) {
        for (const auto& event_fd : selection.event_fds) {
          if (event_fd->Cpu() == cpu) {
            event_id_data.push_back(attr_id);
            event_id_data.push_back(event_fd->Id());
          }
        }
        ++attr_id;
      }
    }
    EventIdRecord r(event_id_data);
    if (!record_callback_(&r)) {
      return false;
    }
  }
  return true;
}

bool EventSelectionSet::CreateMappedBufferForCpu(int cpu) {
  EventFd* fd_with_buffer = nullptr;
  for (auto& group : groups_) {
    for (auto& selection : group) {
      for (auto& event_fd : selection.event_fds) {
        if (event_fd->Cpu() != cpu) {
          continue;
        }
        if (fd_with_buffer == nullptr) {
          if (!event_fd->CreateMappedBuffer(mmap_pages_, true)) {
            return false;
          }
          fd_with_buffer = event_fd.get();
        } else {
          if (!event_fd->ShareMappedBuffer(*fd_with_buffer, true)) {
            fd_with_buffer->DestroyMappedBuffer();
            return false;
          }
        }
      }
    }
  }
  if (fd_with_buffer != nullptr &&
      !fd_with_buffer->StartPolling(*loop_, [this, fd_with_buffer]() {
        return ReadMmapEventDataForFd(fd_with_buffer);
      })) {
    return false;
  }
  return true;
}
