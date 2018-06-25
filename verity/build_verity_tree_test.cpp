/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <gtest/gtest.h>

#include <stdlib.h>

#include <string>
#include <vector>

#include "build_verity_tree_utils.h"
#include "hash_tree_builder.h"

class BuildVerityTreeTest : public ::testing::Test {
 protected:
  BuildVerityTreeTest() : hasher(4096) {}

  void SetUp() override {
    // The hex string we are using in build_image.py
    // aee087a5be3b982978c923f566a94613496b417f2af592639bc80d141e34dfe7
    salt_hex = {0xae, 0xe0, 0x87, 0xa5, 0xbe, 0x3b, 0x98, 0x29,
                0x78, 0xc9, 0x23, 0xf5, 0x66, 0xa9, 0x46, 0x13,
                0x49, 0x6b, 0x41, 0x7f, 0x2a, 0xf5, 0x92, 0x63,
                0x9b, 0xc8, 0x0d, 0x14, 0x1e, 0x34, 0xdf, 0xe7};
  }

  void generate_hash_tree(const std::vector<unsigned char>& data,
                          const std::vector<unsigned char>& salt) {
    ASSERT_TRUE(hasher.Initialize(data.size(), salt));
    ASSERT_TRUE(hasher.Update(data.data(), data.size()));
    ASSERT_TRUE(hasher.BuildHashTree());
  }

  std::vector<unsigned char> salt_hex;
  HashTreeBuilder hasher;
};

TEST_F(BuildVerityTreeTest, InitializeHasher) {
  // data_size should be divisible by 4096
  ASSERT_FALSE(hasher.Initialize(4095, salt_hex));

  ASSERT_TRUE(hasher.Initialize(4096, salt_hex));
  ASSERT_EQ(1u, hasher.verity_tree().size());
  ASSERT_EQ("6eb8c4e1bce842d137f18b27beb857d3b43899d178090537ad7a0fbe3bf4126a",
            HexToString(hasher.zero_block_hash()));
}

TEST_F(BuildVerityTreeTest, HashSingleBlock) {
  std::vector<unsigned char> data(4096, 1);

  generate_hash_tree(data, salt_hex);

  ASSERT_EQ(1u, hasher.verity_tree().size());
  ASSERT_EQ("e69eb527b16f933483768e92de9bca45f6cc09208525d408436bb362eb865d32",
            HexToString(hasher.root_hash()));
}

TEST_F(BuildVerityTreeTest, HashSingleLevel) {
  std::vector<unsigned char> data(128 * 4096, 0);

  generate_hash_tree(data, salt_hex);

  ASSERT_EQ(1u, hasher.verity_tree().size());
  ASSERT_EQ("62a4fbe8c9036168ba77fe3e3fd78dd6ed963aeb8aaaa36e84f5c7f9107c6b78",
            HexToString(hasher.root_hash()));
}

TEST_F(BuildVerityTreeTest, HashMultipleLevels) {
  std::vector<unsigned char> data(129 * 4096, 0xff);

  generate_hash_tree(data, salt_hex);

  ASSERT_EQ(2u, hasher.verity_tree().size());
  ASSERT_EQ(2 * 4096u, hasher.verity_tree()[0].size());
  ASSERT_EQ("9e74f2d47a990c276093760f01de5e9039883e808286ee9492c9cafe9e4ff825",
            HexToString(hasher.root_hash()));
}

TEST_F(BuildVerityTreeTest, StreamingDataMultipleBlocks) {
  std::vector<unsigned char> data;
  for (size_t i = 0; i < 256; i++) {
    std::vector<unsigned char> buffer(4096, i);
    data.insert(data.end(), buffer.begin(), buffer.end());
  }

  ASSERT_TRUE(hasher.Initialize(data.size(), salt_hex));

  size_t offset = 0;
  while (offset < data.size()) {
    size_t data_length =
        std::min<size_t>(rand() % 10 * 4096, data.size() - offset);
    ASSERT_TRUE(hasher.Update(data.data() + offset, data_length));
    offset += data_length;
  }

  ASSERT_TRUE(hasher.BuildHashTree());
  ASSERT_EQ(2u, hasher.verity_tree().size());
  ASSERT_EQ(2 * 4096u, hasher.verity_tree()[0].size());
  ASSERT_EQ("6e73d59b0b6baf026e921814979a7db02244c95a46b869a17aa1310dad066deb",
            HexToString(hasher.root_hash()));
}
