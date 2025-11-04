////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#if defined(ORK_IOS)

#include <ork/util/upload.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/timer.h>
#include <fstream>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <memory>

namespace ork {

////////////////////////////////////////////////////////////////////////////////
// HttpsUploader
////////////////////////////////////////////////////////////////////////////////

// iOS stub Impl
struct HttpsUploader::Impl {
  ~Impl() = default;
};

HttpsUploader::HttpsUploader(httpsuploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
  _log_channel = logger()->configureChannel("HTTPSUPLOAD",fvec3(1,.7,1),true);
}

HttpsUploader::~HttpsUploader() = default;

bool HttpsUploader::uploadFiles(
    const std::vector<file::Path>& local_files,
    const std::vector<std::string>& remote_paths) {
  return false;  // Not supported on iOS
}

bool HttpsUploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  return false;  // Not supported on iOS
}

bool HttpsUploader::testConnection() {
  return false;
}

bool HttpsUploader::remoteFileExists(const std::string& remote_path) {
  return false;
}

bool HttpsUploader::deleteRemoteFile(const std::string& remote_path) {
  return false;
}

void HttpsUploader::setCustomHeaders(const std::map<std::string, std::string>& headers) {
}

void HttpsUploader::setEndpointUrl(const URL& url) {
  // Not supported on iOS
}

void HttpsUploader::handleUploadProgress(size_t uploaded, size_t total) {
}

bool HttpsUploader::remoteFileMatchesLocal(const file::Path& local_file, const std::string& remote_path) {
  return false;
}

std::vector<std::string> HttpsUploader::listRemoteDirectory(const std::string& remote_path) {
  return {};  // Not supported on iOS
}

} //  namespace ork {

#endif // ORK_IOS
