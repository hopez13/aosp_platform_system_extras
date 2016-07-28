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

#include "avb_hashtree_descriptor.h"
#include "avb_util.h"

int avb_hashtree_descriptor_validate_and_byteswap(
    const AvbHashtreeDescriptor* src, AvbHashtreeDescriptor* dest) {
  uint64_t expected_size;

  avb_memcpy(dest, src, sizeof(AvbHashtreeDescriptor));

  if (!avb_descriptor_validate_and_byteswap((const AvbDescriptor*)src,
                                            (AvbDescriptor*)dest))
    return 0;

  if (dest->parent_descriptor.tag != AVB_DESCRIPTOR_TAG_HASHTREE) {
    avb_warning("Invalid tag %" PRIu64 " for hashtree descriptor.\n",
                dest->parent_descriptor.tag);
    return 0;
  }

  dest->dm_verity_version = avb_be32toh(dest->dm_verity_version);
  dest->image_size = avb_be64toh(dest->image_size);
  dest->tree_offset = avb_be64toh(dest->tree_offset);
  dest->tree_size = avb_be64toh(dest->tree_size);
  dest->data_block_size = avb_be32toh(dest->data_block_size);
  dest->hash_block_size = avb_be32toh(dest->hash_block_size);
  dest->partition_name_len = avb_be32toh(dest->partition_name_len);
  dest->salt_len = avb_be32toh(dest->salt_len);
  dest->root_hash_len = avb_be32toh(dest->root_hash_len);

  // Check that partition_name, salt, and root_hash are fully contained.
  expected_size = sizeof(AvbHashtreeDescriptor) - sizeof(AvbDescriptor);
  if (!avb_safe_add_to(&expected_size, dest->partition_name_len) ||
      !avb_safe_add_to(&expected_size, dest->salt_len) ||
      !avb_safe_add_to(&expected_size, dest->root_hash_len)) {
    avb_warning("Overflow while adding up sizes.\n");
    return 0;
  }
  if (expected_size > dest->parent_descriptor.num_bytes_following) {
    avb_warning("Descriptor payload size overflow.\n");
    return 0;
  }
  return 1;
}
