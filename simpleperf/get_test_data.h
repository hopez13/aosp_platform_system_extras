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

#ifndef SIMPLE_PERF_GET_TEST_DATA_H_
#define SIMPLE_PERF_GET_TEST_DATA_H_

#include <string>

#include "build_id.h"

std::string GetTestData(const std::string& filename);
const std::string& GetTestDataDir();

bool IsRoot();

static const std::string PERF_DATA = "perf.data";
static const std::string CALLGRAPH_FP_PERF_DATA = "perf_g_fp.data";
static const std::string BRANCH_PERF_DATA = "perf_b.data";
static const std::string PERF_DATA_WITH_MINI_DEBUG_INFO = "perf_with_mini_debug_info.data";

static const std::string ELF_FILE = "elf";
static const std::string ELF_FILE_WITH_MINI_DEBUG_INFO = "elf_with_mini_debug_info";

static const std::string APK_FILE = "data/app/com.example.hellojni-1/base.apk";
static const std::string NATIVELIB_IN_APK = "lib/arm64-v8a/libhello-jni.so";
static const std::string NATIVELIB_IN_APK_PERF_DATA = "has_embedded_native_libs_apk_perf.data";

constexpr size_t NATIVELIB_OFFSET_IN_APK = 0x8000;
constexpr size_t NATIVELIB_SIZE_IN_APK = 0x15d8;

static BuildId elf_file_build_id("0b12a384a9f4a3f3659b7171ca615dbec3a81f71");

static BuildId native_lib_build_id("b46f51cb9c4b71fb08a2fdbefc2c187894f14008");

#endif  // SIMPLE_PERF_GET_TEST_DATA_H_
