#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

simpleperf_common_cppflags := -Wextra -Wunused -Wno-unknown-pragmas

simpleperf_cppflags_target := $(simpleperf_common_cppflags)

simpleperf_cppflags_host := $(simpleperf_common_cppflags) \
                            -DUSE_BIONIC_UAPI_HEADERS -I bionic/libc/kernel \

simpleperf_cppflags_host_darwin := -I $(LOCAL_PATH)/nonlinux_support/include
simpleperf_cppflags_host_windows := -I $(LOCAL_PATH)/nonlinux_support/include


LLVM_ROOT_PATH := external/llvm
include $(LLVM_ROOT_PATH)/llvm.mk

simpleperf_shared_libraries_target := \
  libbacktrace \
  libbacktrace_offline \
  libbase \
  liblog \
  libutils \
  libLLVM \

simpleperf_static_libraries_target := \
  libziparchive \

simpleperf_shared_libraries_host := libbase

simpleperf_shared_libraries_host_linux := \
  libbacktrace \
  libbacktrace_offline \
  libziparchive-host \

simpleperf_shared_libraries_host_darwin := libLLVM

simpleperf_shared_libraries_host_windows := libLLVM

simpleperf_ldlibs_host_linux := -lrt


# libsimpleperf
# =========================================================
libsimpleperf_src_files := \
  callchain.cpp \
  cmd_dumprecord.cpp \
  cmd_help.cpp \
  cmd_report.cpp \
  command.cpp \
  dso.cpp \
  event_attr.cpp \
  event_type.cpp \
  perf_regs.cpp \
  read_elf.cpp \
  record.cpp \
  record_file_reader.cpp \
  sample_tree.cpp \
  thread_tree.cpp \
  utils.cpp \

libsimpleperf_src_files_linux := \
  cmd_list.cpp \
  cmd_record.cpp \
  cmd_stat.cpp \
  dwarf_unwind.cpp \
  environment.cpp \
  event_fd.cpp \
  event_selection_set.cpp \
  read_apk.cpp \
  record_file_writer.cpp \
  workload.cpp \

libsimpleperf_src_files_darwin := \
  nonlinux_support/nonlinux_support.cpp \

libsimpleperf_src_files_windows := \
  nonlinux_support/nonlinux_support.cpp \

# libsimpleperf target
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := libsimpleperf
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_CPPFLAGS := $(simpleperf_cppflags_target)
LOCAL_SRC_FILES := \
  $(libsimpleperf_src_files) \
  $(libsimpleperf_src_files_linux) \

LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_target)
LOCAL_MULTILIB := first
include $(LLVM_DEVICE_BUILD_MK)
include $(BUILD_STATIC_LIBRARY)

# libsimpleperf host
include $(CLEAR_VARS)
#LOCAL_CLANG := true  # Comment it to build on windows.
LOCAL_MODULE := libsimpleperf
LOCAL_MODULE_HOST_OS := darwin linux windows
LOCAL_CPPFLAGS := $(simpleperf_cppflags_host)
LOCAL_CPPFLAGS_darwin := $(simpleperf_cppflags_host_darwin)
LOCAL_CPPFLAGS_linux := $(simpleperf_cppflags_host_linux)
LOCAL_CPPFLAGS_windows := $(simpleperf_cppflags_host_windows)
LOCAL_SRC_FILES := $(libsimpleperf_src_files)
LOCAL_SRC_FILES_darwin := $(libsimpleperf_src_files_darwin)
LOCAL_SRC_FILES_linux := $(libsimpleperf_src_files_linux)
LOCAL_SRC_FILES_windows := $(libsimpleperf_src_files_windows)
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_host)
LOCAL_SHARED_LIBRARIES_darwin := $(simpleperf_shared_libraries_host_darwin)
LOCAL_SHARED_LIBRARIES_linux := $(simpleperf_shared_libraries_host_linux)
LOCAL_SHARED_LIBRARIES_windows := $(simpleperf_shared_libraries_host_windows)
LOCAL_LDLIBS_linux := $(simpleperf_ldlibs_host_linux)
LOCAL_MULTILIB := first
include $(LLVM_HOST_BUILD_MK)
include $(BUILD_HOST_STATIC_LIBRARY)


# simpleperf
# =========================================================

# simpleperf target
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := simpleperf
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE_PATH := $(TARGET_OUT_OPTIONAL_EXECUTABLES)
LOCAL_CPPFLAGS := $(simpleperf_cppflags_target)
LOCAL_SRC_FILES := main.cpp
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_target)
LOCAL_STATIC_LIBRARIES := $(simpleperf_static_libraries_target)
LOCAL_MULTILIB := first
include $(BUILD_EXECUTABLE)

# simpleperf host
include $(CLEAR_VARS)
LOCAL_MODULE := simpleperf
LOCAL_MODULE_HOST_OS := darwin linux windows
LOCAL_CPPFLAGS := $(simpleperf_cppflags_host)
LOCAL_CPPFLAGS_darwin := $(simpleperf_cppflags_host_darwin)
LOCAL_CPPFLAGS_linux := $(simpleperf_cppflags_host_linux)
LOCAL_CPPFLAGS_windows := $(simpleperf_cppflags_host_windows)
LOCAL_SRC_FILES := main.cpp
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_host)
LOCAL_SHARED_LIBRARIES_darwin := $(simpleperf_shared_libraries_host_darwin)
LOCAL_SHARED_LIBRARIES_linux := $(simpleperf_shared_libraries_host_linux)
LOCAL_SHARED_LIBRARIES_windows := $(simpleperf_shared_libraries_host_windows)
LOCAL_LDLIBS_linux := $(simpleperf_ldlibs_host_linux)
LOCAL_MULTILIB := first
include $(BUILD_HOST_EXECUTABLE)


# simpleperf_unit_test
# =========================================================
simpleperf_unit_test_src_files := \
  command_test.cpp \
  gtest_main.cpp \
  record_test.cpp \
  sample_tree_test.cpp \

simpleperf_unit_test_src_files_linux := \
  cmd_dumprecord_test.cpp \
  cmd_list_test.cpp \
  cmd_record_test.cpp \
  cmd_report_test.cpp \
  cmd_stat_test.cpp \
  environment_test.cpp \
  read_elf_test.cpp \
  read_apk_test.cpp \
  record_file_test.cpp \
  workload_test.cpp \

# simpleperf_unit_test target
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := simpleperf_unit_test
LOCAL_CPPFLAGS := $(simpleperf_cppflags_target)
LOCAL_SRC_FILES := \
  $(simpleperf_unit_test_src_files) \
  $(simpleperf_unit_test_src_files_linux) \

LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_target)
LOCAL_STATIC_LIBRARIES += $(simpleperf_static_libraries_target)
LOCAL_MULTILIB := first
include $(BUILD_NATIVE_TEST)

# simpleperf_unit_test host
include $(CLEAR_VARS)
LOCAL_MODULE := simpleperf_unit_test
LOCAL_MODULE_HOST_OS := darwin linux windows
LOCAL_CPPFLAGS := $(simpleperf_cppflags_host)
LOCAL_CPPFLAGS_darwin := $(simpleperf_cppflags_host_darwin)
LOCAL_CPPFLAGS_linux := $(simpleperf_cppflags_host_linux)
LOCAL_CPPFLAGS_windows := $(simpleperf_cppflags_host_windows)
LOCAL_SRC_FILES := $(simpleperf_unit_test_src_files)
LOCAL_SRC_FILES_linux := $(simpleperf_unit_test_src_files_linux)
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_host)
LOCAL_SHARED_LIBRARIES_darwin := $(simpleperf_shared_libraries_host_darwin)
LOCAL_SHARED_LIBRARIES_linux := $(simpleperf_shared_libraries_host_linux)
LOCAL_SHARED_LIBRARIES_windows := $(simpleperf_shared_libraries_host_windows)
LOCAL_LDLIBS_linux := $(simpleperf_ldlibs_host_linux)
LOCAL_MULTILIB := first
include $(BUILD_HOST_NATIVE_TEST)


# simpleperf_cpu_hotplug_test
# =========================================================
simpleperf_cpu_hotplug_test_src_files := \
  gtest_main.cpp \
  cpu_hotplug_test.cpp \

# simpleperf_cpu_hotplug_test target
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := simpleperf_cpu_hotplug_test
LOCAL_CPPFLAGS := $(simpleperf_cppflags_target)
LOCAL_SRC_FILES := $(simpleperf_cpu_hotplug_test_src_files)
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_target)
LOCAL_STATIC_LIBRARIES := $(simpleperf_static_libraries_target)
LOCAL_MULTILIB := first
include $(BUILD_NATIVE_TEST)

# simpleperf_cpu_hotplug_test linux host
include $(CLEAR_VARS)
LOCAL_CLANG := true
LOCAL_MODULE := simpleperf_cpu_hotplug_test
LOCAL_MODULE_HOST_OS := linux
LOCAL_CPPFLAGS := $(simpleperf_cppflags_host)
LOCAL_CPPFLAGS_linux := $(simpleperf_cppflags_host_linux)
LOCAL_SRC_FILES := $(simpleperf_cpu_hotplug_test_src_files)
LOCAL_WHOLE_STATIC_LIBRARIES := libsimpleperf
LOCAL_SHARED_LIBRARIES := $(simpleperf_shared_libraries_host)
LOCAL_SHARED_LIBRARIES_linux := $(simpleperf_shared_libraries_host_linux)
LOCAL_LDLIBS_linux := $(simpleperf_ldlibs_host_linux)
LOCAL_MULTILIB := first
include $(BUILD_HOST_NATIVE_TEST)

include $(call first-makefiles-under,$(LOCAL_PATH))
