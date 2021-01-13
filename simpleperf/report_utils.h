/*
 * Copyright (C) 2020 The Android Open Source Project
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

#pragma once

#include <inttypes.h>

#include <string_view>
#include <vector>

#include "dso.h"
#include "thread_tree.h"

namespace simpleperf {

enum class CallChainExecutionType {
  NATIVE_METHOD,
  INTERPRETED_JVM_METHOD,
  JIT_JVM_METHOD,
  // ART methods near interpreted/JIT JVM methods. They're shown only when RemoveArtFrame = false.
  ART_METHOD,
  // JNI native methods implemented in libart.so
  ART_JNI_METHOD,
};

struct CallChainReportEntry {
  uint64_t ip = 0;
  const Symbol* symbol = nullptr;
  const char* symbol_name = nullptr;
  Dso* dso = nullptr;
  const char* dso_name = nullptr;
  uint64_t vaddr_in_file = 0;
  const MapEntry* map = nullptr;
  CallChainExecutionType execution_type = CallChainExecutionType::NATIVE_METHOD;
};

class CallChainReportBuilder {
 public:
  CallChainReportBuilder(ThreadTree& thread_tree);
  // If true, remove interpreter frames both before and after a Java frame.
  // Default is true.
  void SetRemoveArtFrame(bool enable) { remove_art_frame_ = enable; }
  // If true, convert a JIT method into its corresponding interpreted Java method. So they can be
  // merged in reports like flamegraph. Default is true.
  void SetConvertJITFrame(bool enable) { convert_jit_frame_ = enable; }
  // If true, convert ART JNI methods to their corresponding Java method names.
  // For example, art::Method_invoke is converted to java.lang.reflect.Method.invoke.
  // Default is false.
  void SetConvertArtJniMethod(bool enable) { convert_art_jni_method_ = enable; }
  std::vector<CallChainReportEntry> Build(const ThreadEntry* thread,
                                          const std::vector<uint64_t>& ips, size_t kernel_ip_count);

 private:
  struct JavaMethod {
    Dso* dso;
    const Symbol* symbol;
    JavaMethod(Dso* dso, const Symbol* symbol) : dso(dso), symbol(symbol) {}
  };

  void MarkArtFrame(std::vector<CallChainReportEntry>& callchain);
  void ConvertJITFrame(std::vector<CallChainReportEntry>& callchain);
  void CollectJavaMethods();

  ThreadTree& thread_tree_;
  bool remove_art_frame_ = true;
  bool convert_jit_frame_ = true;
  bool convert_art_jni_method_ = false;
  bool java_method_initialized_ = false;
  std::unordered_map<std::string, JavaMethod> java_method_map_;
  std::unordered_map<std::string_view, const char*> art_jni_method_map_;
};

}  // namespace simpleperf
