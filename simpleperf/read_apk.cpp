/*
**
** Copyright 2016, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include "read_apk.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <ziparchive/zip_archive.h>

#include "read_elf.h"
#include "utils.h"

class ArchiveHelper {
 public:
  explicit ArchiveHelper(int fd, const std::string& debug_filename) : valid_(false) {
    int rc = OpenArchiveFd(fd, "", &handle_, false);
    if (rc == 0) {
      valid_ = true;
    } else {
      LOG(ERROR) << "Failed to open archive " << debug_filename << ": " << ErrorCodeString(rc);
    }
  }
  ~ArchiveHelper() {
    if (valid_) {
      CloseArchive(handle_);
    }
  }
  bool valid() const { return valid_; }
  ZipArchiveHandle &archive_handle() { return handle_; }

 private:
  ZipArchiveHandle handle_;
  bool valid_;
};

EmbeddedElf* ApkInspector::FindElfInApkByOffset(const std::string& apk_path, off64_t file_offset) {
  // Already in cache?
  ApkOffset ami(apk_path, file_offset);
  auto it = cache_.find(ami);
  if (it != cache_.end()) {
    uint32_t idx = it->second;
    return (idx ? &embedded_elf_files_[idx-1] : nullptr);
  }
  cache_[ami] = 0u;

  // Crack open the apk(zip) file and take a look.
  if (!IsValidApkPath(apk_path)) {
    return nullptr;
  }
  FileHelper fhelper(apk_path.c_str());
  if (!fhelper) {
    return nullptr;
  }

  ArchiveHelper ahelper(fhelper.fd(), apk_path);
  if (!ahelper.valid()) {
    return nullptr;
  }
  ZipArchiveHandle &handle = ahelper.archive_handle();

  // Iterate through the zip file. Look for a zip entry corresponding
  // to an uncompressed blob whose range intersects with the mmap
  // offset we're interested in.
  void* iteration_cookie;
  if (StartIteration(handle, &iteration_cookie, nullptr, nullptr) < 0) {
    return nullptr;
  }
  ZipEntry zentry;
  ZipString zname;
  bool found = false;
  int zrc;
  while ((zrc = Next(iteration_cookie, &zentry, &zname)) == 0) {
    if (zentry.method == kCompressStored &&
        file_offset >= zentry.offset &&
        file_offset < zentry.offset + zentry.uncompressed_length) {
      // Found.
      found = true;
      break;
    }
  }
  EndIteration(iteration_cookie);
  if (!found) {
    return nullptr;
  }

  // We found something in the zip file at the right spot. Is it an ELF?
  if (lseek(fhelper.fd(), zentry.offset, SEEK_SET) != zentry.offset) {
    PLOG(ERROR) << "lseek() failed in " << apk_path << " offset " << zentry.offset;
    return nullptr;
  }
  std::string entry_name;
  entry_name.resize(zname.name_length,'\0');
  memcpy(&entry_name[0], zname.name, zname.name_length);
  if (!IsValidElfFile(fhelper.fd())) {
    LOG(ERROR) << "problems reading ELF from in " << apk_path << " entry '"
               << entry_name << "'";
    return nullptr;
  }

  // Elf found: add EmbeddedElf to vector, update cache.
  EmbeddedElf ee(apk_path, entry_name, zentry.offset, zentry.uncompressed_length);
  embedded_elf_files_.push_back(ee);
  unsigned idx = embedded_elf_files_.size();
  cache_[ami] = idx;
  return &embedded_elf_files_[idx-1];
}

std::unique_ptr<EmbeddedElf> ApkInspector::FindElfInApkByName(const std::string& apk_path,
                                                              const std::string& elf_filename) {
  if (!IsValidApkPath(apk_path)) {
    return nullptr;
  }
  FileHelper fhelper(apk_path.c_str());
  if (!fhelper) {
    return nullptr;
  }
  ArchiveHelper ahelper(fhelper.fd(), apk_path);
  if (!ahelper.valid()) {
    return nullptr;
  }
  ZipArchiveHandle& handle = ahelper.archive_handle();
  ZipEntry zentry;
  int32_t rc = FindEntry(handle, ZipString(elf_filename.c_str()), &zentry);
  if (rc != 0) {
    LOG(ERROR) << "failed to find " << elf_filename << " in " << apk_path
        << ": " << ErrorCodeString(rc);
    return nullptr;
  }
  if (zentry.method != kCompressStored || zentry.compressed_length != zentry.uncompressed_length) {
    LOG(ERROR) << "shared library " << elf_filename << " in " << apk_path << " is compressed";
    return nullptr;
  }
  return std::unique_ptr<EmbeddedElf>(new EmbeddedElf(apk_path, elf_filename, zentry.offset,
                                                  zentry.uncompressed_length));
}

bool IsValidApkPath(const std::string& apk_path) {
  static const char zip_preamble[] = {0x50, 0x4b, 0x03, 0x04 };
  if (!IsRegularFile(apk_path)) {
    return false;
  }
  std::string mode = std::string("rb") + CLOSE_ON_EXEC_MODE;
  FILE* fp = fopen(apk_path.c_str(), mode.c_str());
  if (fp == nullptr) {
    return false;
  }
  char buf[4];
  if (fread(buf, 4, 1, fp) != 1) {
    fclose(fp);
    return false;
  }
  fclose(fp);
  return memcmp(buf, zip_preamble, 4) == 0;
}

bool GetBuildIdFromApkFile(const std::string& apk_path, const std::string& elf_filename,
                           BuildId* build_id) {
  std::unique_ptr<EmbeddedElf> ee = ApkInspector::FindElfInApkByName(apk_path, elf_filename);
  if (ee == nullptr) {
    return false;
  }
  return GetBuildIdFromEmbeddedElfFile(apk_path, ee->entry_offset(), ee->entry_size(), build_id);
}

bool ParseSymbolsFromApkFile(const std::string& apk_path, const std::string& elf_filename,
                             const BuildId& expected_build_id,
                             std::function<void(const ElfFileSymbol&)> callback) {
  std::unique_ptr<EmbeddedElf> ee = ApkInspector::FindElfInApkByName(apk_path, elf_filename);
  if (ee == nullptr) {
    return false;
  }
  return ParseSymbolsFromEmbeddedElfFile(apk_path, ee->entry_offset(), ee->entry_size(),
                                         expected_build_id, callback);
}
