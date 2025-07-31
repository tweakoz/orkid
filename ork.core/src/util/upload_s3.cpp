////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/upload.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <ork/util/logger.h>
#include <curl/curl.h>
#include <rapidjson/document.h>
#include <fstream>
#include <atomic>
#include <mutex>

namespace ork {

  struct S3Uploader::Impl {
  // TODO: Add S3 SDK state
};

  ////////////////////////////////////////////////////////////////////////////////
// S3Uploader
////////////////////////////////////////////////////////////////////////////////

S3Uploader::S3Uploader(s3uploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
}

S3Uploader::~S3Uploader() = default;

bool S3Uploader::uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths) {
  // TODO: Implement batch upload
  for (size_t i = 0; i < local_files.size(); ++i) {
    if (!uploadFile(local_files[i], remote_paths[i])) {
      return false;
    }
  }
  return true;
}

bool S3Uploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  // TODO: Implement S3 upload using AWS SDK
  return false;
}

bool S3Uploader::testConnection() {
  // TODO: Test S3 connection
  return true;
}

bool S3Uploader::remoteFileExists(const std::string& remote_path) {
  // TODO: Check via S3 HEAD request
  return false;
}

bool S3Uploader::deleteRemoteFile(const std::string& remote_path) {
  // TODO: Delete via S3 DELETE
  return false;
}

std::vector<std::string> S3Uploader::listRemoteDirectory(const std::string& path) {
  // TODO: List via S3 API
  return {};
}

void S3Uploader::setBucket(const std::string& bucket) {
  _config->bucket = bucket;
}

void S3Uploader::setRegion(const std::string& region) {
  _config->region = region;
}

void S3Uploader::setStorageClass(const std::string& storage_class) {
  _config->storage_class = storage_class;
}

void S3Uploader::setACL(const std::string& acl) {
  _config->acl = acl;
}


} // 