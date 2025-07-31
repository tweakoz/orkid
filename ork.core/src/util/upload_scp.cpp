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

  ////////////////////////////////////////////////////////////////////////////////
// ScpUploader
////////////////////////////////////////////////////////////////////////////////
struct ScpUploader::Impl {
  // TODO: Add SSH/SCP state
};

ScpUploader::ScpUploader(scpuploaderconfig_ptr_t config)
    : _config(config) {
  _impl = std::make_unique<Impl>();
}

ScpUploader::~ScpUploader() = default;

bool ScpUploader::uploadFiles(
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

bool ScpUploader::uploadFile(
    const file::Path& local_file,
    const std::string& remote_path) {
  // TODO: Implement SCP upload
  return false;
}

bool ScpUploader::testConnection() {
  // TODO: Test SSH connection
  return true;
}

bool ScpUploader::remoteFileExists(const std::string& remote_path) {
  // TODO: Check via SSH
  return false;
}

bool ScpUploader::deleteRemoteFile(const std::string& remote_path) {
  // TODO: Delete via SSH
  return false;
}

std::vector<std::string> ScpUploader::listRemoteDirectory(const std::string& path) {
  // TODO: List via SSH
  return {};
}

bool ScpUploader::hasControlMaster() const {
  if (_config->control_path.empty()) {
    return false;
  }
  // TODO: Check if control socket exists
  return false;
}

std::string ScpUploader::getControlPath() const {
  if (!_config->control_path.empty()) {
    return _config->control_path;
  }
  // Generate default control path
  return FormatString("~/.ssh/cm-%s-%d-%s", 
    _config->host.c_str(), 
    _config->port, 
    _config->username.c_str());
}

std::string ScpUploader::getControlMasterSetupInstructions(
    const std::string& host,
    const std::string& username,
    int port) {
  return FormatString(
    "To set up SSH ControlMaster for 2FA:\n"
    "1. Open a new terminal\n"
    "2. Run: ssh -M -S ~/.ssh/cm-%s-%d-%s %s@%s -p %d\n"
    "3. Enter your password and 2FA code when prompted\n"
    "4. Keep this terminal open while uploading\n",
    host.c_str(), port, username.c_str(),
    username.c_str(), host.c_str(), port
  );
}

std::vector<std::string> ScpUploader::buildScpCommand(
    const file::Path& local_file,
    const std::string& remote_path,
    bool upload) const {
  std::vector<std::string> cmd;
  cmd.push_back("scp");
  
  // Add common SSH options
  auto options = getSshOptions();
  cmd.insert(cmd.end(), options.begin(), options.end());
  
  // Add port
  cmd.push_back("-P");
  cmd.push_back(std::to_string(_config->port));
  
  // Add source and destination
  if (upload) {
    cmd.push_back(local_file.c_str());
    cmd.push_back(FormatString("%s@%s:%s", 
      _config->username.c_str(), 
      _config->host.c_str(), 
      remote_path.c_str()));
  } else {
    cmd.push_back(FormatString("%s@%s:%s", 
      _config->username.c_str(), 
      _config->host.c_str(), 
      remote_path.c_str()));
    cmd.push_back(local_file.c_str());
  }
  
  return cmd;
}

std::vector<std::string> ScpUploader::buildSshCommand(
    const std::string& remote_command) const {
  std::vector<std::string> cmd;
  cmd.push_back("ssh");
  
  // Add common SSH options
  auto options = getSshOptions();
  cmd.insert(cmd.end(), options.begin(), options.end());
  
  // Add port
  cmd.push_back("-p");
  cmd.push_back(std::to_string(_config->port));
  
  // Add target
  cmd.push_back(FormatString("%s@%s", 
    _config->username.c_str(), 
    _config->host.c_str()));
  
  // Add command
  cmd.push_back(remote_command);
  
  return cmd;
}

std::vector<std::string> ScpUploader::getSshOptions() const {
  std::vector<std::string> options;
  
  if (_config->use_control_master) {
    options.push_back("-o");
    options.push_back("ControlMaster=no");
    options.push_back("-o");
    options.push_back(FormatString("ControlPath=%s", getControlPath().c_str()));
  }
  
  if (_config->auto_add_host_key) {
    options.push_back("-o");
    options.push_back("StrictHostKeyChecking=no");
  }
  
  if (!_config->ssh_options.empty()) {
    // Parse additional options
    // TODO: Properly parse ssh_options string
  }
  
  return options;
}

} // namespace ork