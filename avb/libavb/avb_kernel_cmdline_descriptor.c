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

#include "avb_kernel_cmdline_descriptor.h"
#include "avb_util.h"

int avb_kernel_cmdline_descriptor_validate_and_byteswap(
    const AvbKernelCmdlineDescriptor* src, AvbKernelCmdlineDescriptor* dest) {
  uint64_t expected_size;

  avb_memcpy(dest, src, sizeof(AvbKernelCmdlineDescriptor));

  if (!avb_descriptor_validate_and_byteswap((const AvbDescriptor*)src,
                                            (AvbDescriptor*)dest))
    return 0;

  if (dest->parent_descriptor.tag != AVB_DESCRIPTOR_TAG_KERNEL_CMDLINE) {
    avb_error("Invalid tag %" PRIu64 " for kernel cmdline descriptor.\n",
              dest->parent_descriptor.tag);
    return 0;
  }

  dest->kernel_cmdline_length = avb_be32toh(dest->kernel_cmdline_length);

  /* Check that kernel_cmdline is fully contained. */
  expected_size = sizeof(AvbKernelCmdlineDescriptor) - sizeof(AvbDescriptor);
  if (!avb_safe_add_to(&expected_size, dest->kernel_cmdline_length)) {
    avb_error("Overflow while adding up sizes.\n");
    return 0;
  }
  if (expected_size > dest->parent_descriptor.num_bytes_following) {
    avb_error("Descriptor payload size overflow.\n");
    return 0;
  }

  return 1;
}
