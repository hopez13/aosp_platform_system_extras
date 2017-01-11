# Copyright 2010 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)

libext4_utils_src_files := \
    make_ext4fs.c \
    ext4fixup.c \
    ext4_utils.c \
    allocate.c \
    contents.c \
    extent.c \
    indirect.c \
    sha1.c \
    wipe.c \
    crc16.c \
    ext4_sb.c

#
# -- All host/targets including windows
#

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils
# Various instances of dereferencing a type-punned pointer in extent.c
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    libsparse
LOCAL_STATIC_LIBRARIES_darwin += libselinux
LOCAL_STATIC_LIBRARIES_linux += libselinux
LOCAL_MODULE_HOST_OS := darwin linux windows
include $(BUILD_HOST_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.c
LOCAL_MODULE := make_ext4fs
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_STATIC_LIBRARIES += \
    libext4_utils \
    libsparse \
    libz
LOCAL_LDLIBS_windows += -lws2_32
LOCAL_SHARED_LIBRARIES_darwin += libselinux
LOCAL_SHARED_LIBRARIES_linux += libselinux
LOCAL_CFLAGS_darwin := -DHOST
LOCAL_CFLAGS_linux := -DHOST
include $(BUILD_HOST_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := blk_alloc_to_base_fs.c
LOCAL_MODULE := blk_alloc_to_base_fs
LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_CFLAGS_darwin := -DHOST
LOCAL_CFLAGS_linux := -DHOST
include $(BUILD_HOST_EXECUTABLE)

#
# -- All host/targets excluding windows
#

libext4_utils_src_files += \
    key_control.cpp \
    ext4_crypt.cpp

ifneq ($(HOST_OS),windows)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(libext4_utils_src_files)
LOCAL_MODULE := libext4_utils
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include \
    system/core/logwrapper/include
# Various instances of dereferencing a type-punned pointer in extent.c
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_CFLAGS += -DREAL_UUID
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := \
    libbase \
    libcutils \
    libext2_uuid \
    libselinux \
    libsparse
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    $(libext4_utils_src_files) \
    ext4_crypt_init_extensions.cpp
LOCAL_MODULE := libext4_utils
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
# Various instances of dereferencing a type-punned pointer in extent.c
LOCAL_CFLAGS += -fno-strict-aliasing
LOCAL_EXPORT_C_INCLUDE_DIRS := \
    $(LOCAL_PATH)/include
LOCAL_STATIC_LIBRARIES := \
    liblogwrap \
    libsparse \
    libselinux \
    libbase
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := make_ext4fs_main.c
LOCAL_MODULE := make_ext4fs
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/include
LOCAL_SHARED_LIBRARIES := \
    libcutils \
    libext2_uuid \
    libext4_utils \
    libselinux \
    libz
LOCAL_CFLAGS := -DREAL_UUID
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := setup_fs.c
LOCAL_MODULE := setup_fs
LOCAL_SHARED_LIBRARIES += libcutils
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.c
LOCAL_MODULE := ext4fixup
LOCAL_SHARED_LIBRARIES += \
    libext4_utils \
    libsparse \
    libz
include $(BUILD_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_SRC_FILES := ext4fixup_main.c
LOCAL_MODULE := ext4fixup
LOCAL_STATIC_LIBRARIES += \
    libext4_utils \
    libsparse \
    libz
include $(BUILD_HOST_EXECUTABLE)


include $(CLEAR_VARS)
LOCAL_MODULE := mkuserimg.sh
LOCAL_SRC_FILES := mkuserimg.sh
LOCAL_MODULE_CLASS := EXECUTABLES
# We don't need any additional suffix.
LOCAL_MODULE_SUFFIX :=
LOCAL_BUILT_MODULE_STEM := $(notdir $(LOCAL_SRC_FILES))
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE := mkuserimg_mke2fs.sh
LOCAL_SRC_FILES := mkuserimg_mke2fs.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_REQUIRED_MODULES := mke2fs e2fsdroid
# We don't need any additional suffix.
LOCAL_MODULE_SUFFIX :=
LOCAL_BUILT_MODULE_STEM := $(notdir $(LOCAL_SRC_FILES))
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PREBUILT)

endif
