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

#include <stdio.h>
#include <map>
#include <string>
#include <vector>

#include <base/logging.h>

#include "command.h"
#include "event_attr.h"
#include "event_fd.h"
#include "event_type.h"

static void PrintEventTypesOfType(uint32_t type, const std::string& type_name,
                                  const std::vector<EventType>& event_types) {
  printf("List of %s:\n", type_name.c_str());
  for (auto& event_type : event_types) {
    if (event_type.type == type &&
        IsEventAttrSupportedByKernel(CreateDefaultPerfEventAttr(event_type))) {
      printf("  %s\n", event_type.name.c_str());
    }
  }
  printf("\n");
}

class ListCommand : public Command {
 public:
  ListCommand()
      : Command("list", "list available event types",
                "Usage: simpleperf list [hw|sw|cache|tracepoint]\n"
                "    List all available perf events on this machine.\n") {
  }

  bool Run(const std::vector<std::string>& args) override;
};

bool ListCommand::Run(const std::vector<std::string>& args) {
  static std::map<std::string, std::pair<int, std::string>> type_map = {
      {"hw", {PERF_TYPE_HARDWARE, "hardware events"}},
      {"sw", {PERF_TYPE_SOFTWARE, "software events"}},
      {"cache", {PERF_TYPE_HW_CACHE, "hw-cache events"}},
      {"tracepoint", {PERF_TYPE_TRACEPOINT, "tracepoint events"}},
  };

  std::vector<std::string> names;
  if (args.empty()) {
    for (auto& item : type_map) {
      names.push_back(item.first);
    }
  } else {
    for (auto& arg : args) {
      if (type_map.find(arg) != type_map.end()) {
        names.push_back(arg);
      } else {
        LOG(ERROR) << "unknown event type category: " << arg << ", try using \"help list\"";
        return false;
      }
    }
  }

  auto& event_types = GetAllEventTypes();

  for (auto& name : names) {
    auto it = type_map.find(name);
    PrintEventTypesOfType(it->second.first, it->second.second, event_types);
  }
  return true;
}

__attribute__((constructor)) static void RegisterListCommand() {
  RegisterCommand("list", [] { return std::unique_ptr<Command>(new ListCommand); });
}
