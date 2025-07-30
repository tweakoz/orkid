////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/upload.h>
#include <ork/util/upload_manager.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/uploader.h>
#include <ork/kernel/opq.h>

namespace ork {

void pyinit_upload(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // UploadManager
  /////////////////////////////////////////////////////////////////////////////////
  auto upload_manager_type = py::class_<UploadManager, uploadmanager_ptr_t>(module_core, "UploadManager")
    .def(py::init<>())
    .def(py::init<opq::opq_ptr_t>(), py::arg("queue") = nullptr)
    .def("upload", &UploadManager::upload, py::arg("source_path"), py::arg("dest_url"))
    .def("setMaxConcurrentUploads", &UploadManager::setMaxConcurrentUploads)
    .def("shutdown", &UploadManager::shutdown)
    .def("isActive", &UploadManager::isActive)
    .def("activeUploadCount", &UploadManager::activeUploadCount)
    .def("setDefaultMaxRetries", &UploadManager::setDefaultMaxRetries)
    .def("setDefaultRetryDelay", &UploadManager::setDefaultRetryDelay)
    .def("setDefaultRetryBackoff", &UploadManager::setDefaultRetryBackoff)
    .def("__repr__", [](uploadmanager_ptr_t mgr) -> std::string {
      return FormatString("UploadManager(active=%d)", mgr->activeUploadCount());
    });
  type_codec->registerStdCodec<uploadmanager_ptr_t>(upload_manager_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Upload
  /////////////////////////////////////////////////////////////////////////////////
  auto upload_type = py::class_<Upload, upload_ptr_t>(module_core, "Upload")
    .def(py::init<>())
    .def_readwrite("source_path", &Upload::_source_path)
    .def_readwrite("destination_url", &Upload::_destination_url)
    .def_readwrite("api_key", &Upload::_api_key)
    .def_readwrite("ignore_tls_errors", &Upload::_ignore_tls_errors)
    .def_readwrite("headers", &Upload::_headers)
    .def_readwrite("username", &Upload::_username)
    .def_readwrite("password", &Upload::_password)
    .def_readwrite("ssh_key_path", &Upload::_ssh_key_path)
    .def_readwrite("max_retries", &Upload::_max_retries)
    .def_readwrite("retry_delay_ms", &Upload::_retry_delay_ms)
    .def_readwrite("retry_backoff_multiplier", &Upload::_retry_backoff_multiplier)
    .def_readwrite("max_retry_delay_ms", &Upload::_max_retry_delay_ms)
    .def_property_readonly("state", [](upload_ptr_t ul) { return ul->_state.load(); })
    .def_property_readonly("bytes_uploaded", [](upload_ptr_t ul) { return ul->_bytes_uploaded.load(); })
    .def_property_readonly("total_bytes", [](upload_ptr_t ul) { return ul->_total_bytes; })
    .def_property_readonly("error_message", [](upload_ptr_t ul) { return ul->_error_message; })
    .def_property_readonly("retry_count", [](upload_ptr_t ul) { return ul->_retry_count.load(); })
    .def("execute", &Upload::execute)
    .def("cancel", &Upload::cancel)
    .def("getProgress", &Upload::getProgress)
    .def("getUploadRate", &Upload::getUploadRate)
    .def("shouldRetry", &Upload::shouldRetry)
    .def("getNextRetryDelay", &Upload::getNextRetryDelay)
    .def("__repr__", [](upload_ptr_t ul) -> std::string {
      return FormatString("Upload('%s' -> '%s', state=%s, progress=%.1f%%)", 
        ul->_source_path.c_str(), 
        ul->_destination_url.toString().c_str(),
        ul->_state.load() == UploadState::PENDING ? "PENDING" :
        ul->_state.load() == UploadState::UPLOADING ? "UPLOADING" :
        ul->_state.load() == UploadState::COMPLETED ? "COMPLETED" :
        ul->_state.load() == UploadState::FAILED ? "FAILED" : "CANCELLED",
        ul->getProgress() * 100.0f);
    });
  type_codec->registerStdCodec<upload_ptr_t>(upload_type);

  // Upload State enum
  py::enum_<UploadState>(module_core, "UploadState")
    .value("PENDING", UploadState::PENDING)
    .value("UPLOADING", UploadState::UPLOADING)
    .value("COMPLETED", UploadState::COMPLETED)
    .value("FAILED", UploadState::FAILED)
    .value("CANCELLED", UploadState::CANCELLED);

  /////////////////////////////////////////////////////////////////////////////////
  // Asset Catalog Upload Types
  /////////////////////////////////////////////////////////////////////////////////

  // UploadProgress
  auto upload_progress_type = py::class_<asset::catalog::UploadProgress>(module_core, "UploadProgress")
    .def(py::init<>())
    .def_readwrite("current_file", &asset::catalog::UploadProgress::current_file)
    .def_readwrite("files_completed", &asset::catalog::UploadProgress::files_completed)
    .def_readwrite("total_files", &asset::catalog::UploadProgress::total_files)
    .def_readwrite("bytes_uploaded", &asset::catalog::UploadProgress::bytes_uploaded)
    .def_readwrite("total_bytes", &asset::catalog::UploadProgress::total_bytes)
    .def_readwrite("elapsed_time", &asset::catalog::UploadProgress::elapsed_time)
    .def_readwrite("estimated_time_remaining", &asset::catalog::UploadProgress::estimated_time_remaining)
    .def("getProgressPercent", &asset::catalog::UploadProgress::getProgressPercent)
    .def("getTransferRate", &asset::catalog::UploadProgress::getTransferRate)
    .def("getRateString", &asset::catalog::UploadProgress::getRateString)
    .def("__repr__", [](const asset::catalog::UploadProgress& prog) -> std::string {
      return FormatString("UploadProgress(%.1f%%, %zu/%zu files, rate=%s)", 
        prog.getProgressPercent(),
        prog.files_completed,
        prog.total_files,
        prog.getRateString().c_str());
    });

  // UploadFileEntry
  auto upload_file_entry_type = py::class_<asset::catalog::UploadFileEntry>(module_core, "UploadFileEntry")
    .def(py::init<>())
    .def_readwrite("relative_path", &asset::catalog::UploadFileEntry::relative_path)
    .def_readwrite("remote_path", &asset::catalog::UploadFileEntry::remote_path)
    .def_readwrite("size", &asset::catalog::UploadFileEntry::size)
    .def_readwrite("hash", &asset::catalog::UploadFileEntry::hash)
    .def_readwrite("success", &asset::catalog::UploadFileEntry::success)
    .def_readwrite("error_message", &asset::catalog::UploadFileEntry::error_message);

  // UploadReceipt
  auto upload_receipt_type = py::class_<asset::catalog::UploadReceipt, asset::catalog::uploadreceipt_ptr_t>(module_core, "UploadReceipt")
    .def(py::init<>())
    .def_readwrite("upload_id", &asset::catalog::UploadReceipt::upload_id)
    .def_readwrite("namespace_id", &asset::catalog::UploadReceipt::namespace_id)
    .def_readwrite("manifest_id", &asset::catalog::UploadReceipt::manifest_id)
    .def_readwrite("destination", &asset::catalog::UploadReceipt::destination)
    .def_readwrite("timestamp", &asset::catalog::UploadReceipt::timestamp)
    .def_readwrite("total_files", &asset::catalog::UploadReceipt::total_files)
    .def_readwrite("successful_files", &asset::catalog::UploadReceipt::successful_files)
    .def_readwrite("failed_files", &asset::catalog::UploadReceipt::failed_files)
    .def_readwrite("bytes_uploaded", &asset::catalog::UploadReceipt::bytes_uploaded)
    .def_readwrite("total_duration", &asset::catalog::UploadReceipt::total_duration)
    .def_readwrite("success", &asset::catalog::UploadReceipt::success)
    .def_readwrite("status_message", &asset::catalog::UploadReceipt::status_message)
    .def("toJson", &asset::catalog::UploadReceipt::toJson)
    .def("fromJson", &asset::catalog::UploadReceipt::fromJson)
    .def("saveToFile", &asset::catalog::UploadReceipt::saveToFile)
    .def_static("loadFromFile", &asset::catalog::UploadReceipt::loadFromFile)
    .def("getSummary", &asset::catalog::UploadReceipt::getSummary)
    .def("getFailedFiles", &asset::catalog::UploadReceipt::getFailedFiles)
    .def("__repr__", [](asset::catalog::uploadreceipt_ptr_t rcpt) -> std::string {
      return FormatString("UploadReceipt(id='%s', %zu/%zu files, %s)", 
        rcpt->upload_id.c_str(),
        rcpt->successful_files,
        rcpt->total_files,
        rcpt->success ? "SUCCESS" : "FAILED");
    });
  type_codec->registerStdCodec<asset::catalog::uploadreceipt_ptr_t>(upload_receipt_type);

  // Base UploadConfig
  auto upload_config_type = py::class_<UploadConfig, uploadconfig_ptr_t>(module_core, "UploadConfig")
    .def(py::init<>())
    .def_readwrite("remote_base_path", &UploadConfig::remote_base_path)
    .def_readwrite("create_directories", &UploadConfig::create_directories)
    .def_readwrite("overwrite_existing", &UploadConfig::overwrite_existing)
    .def_readwrite("verify_uploads", &UploadConfig::verify_uploads)
    .def_readwrite("compress_transfer", &UploadConfig::compress_transfer)
    .def_readwrite("timeout_seconds", &UploadConfig::timeout_seconds)
    .def_readwrite("retry_count", &UploadConfig::retry_count)
    .def_readwrite("upload_manager", &UploadConfig::upload_manager)
    .def("isValid", &UploadConfig::isValid)
    .def("getValidationError", &UploadConfig::getValidationError)
    .def("__repr__", [](uploadconfig_ptr_t cfg) -> std::string {
      return FormatString("UploadConfig(base_path='%s', valid=%s)", 
        cfg->remote_base_path.c_str(),
        cfg->isValid() ? "true" : "false");
    });
  type_codec->registerStdCodec<uploadconfig_ptr_t>(upload_config_type);

  // HttpsUploaderConfig
  auto https_config_type = py::class_<HttpsUploaderConfig, httpsuploaderconfig_ptr_t, UploadConfig>(module_core, "HttpsUploaderConfig")
    .def(py::init<>())
    .def_readwrite("host", &HttpsUploaderConfig::host)
    .def_readwrite("api_key", &HttpsUploaderConfig::api_key)
    .def_readwrite("username", &HttpsUploaderConfig::username)
    .def_readwrite("password", &HttpsUploaderConfig::password)
    .def_readwrite("port", &HttpsUploaderConfig::port)
    .def_readwrite("verify_ssl", &HttpsUploaderConfig::verify_ssl)
    .def_readwrite("custom_headers", &HttpsUploaderConfig::custom_headers)
    .def("__repr__", [](httpsuploaderconfig_ptr_t cfg) -> std::string {
      return FormatString("HttpsUploaderConfig(host='%s', port=%d, verify_ssl=%s)", 
        cfg->host.c_str(),
        cfg->port,
        cfg->verify_ssl ? "true" : "false");
    });
  type_codec->registerStdCodec<httpsuploaderconfig_ptr_t>(https_config_type);

  // HttpsUploader
  auto https_uploader_type = py::class_<HttpsUploader, std::shared_ptr<HttpsUploader>>(module_core, "HttpsUploader")
    .def(py::init<httpsuploaderconfig_ptr_t>())
    .def("uploadFile", &HttpsUploader::uploadFile, 
         py::arg("local_file"), py::arg("remote_path"))
    .def("uploadFiles", &HttpsUploader::uploadFiles,
         py::arg("local_files"), py::arg("remote_paths"))
    .def("testConnection", &HttpsUploader::testConnection)
    .def("type", &HttpsUploader::type)
    .def("remoteFileExists", &HttpsUploader::remoteFileExists,
         py::arg("remote_path"))
    .def("deleteRemoteFile", &HttpsUploader::deleteRemoteFile,
         py::arg("remote_path"))
    .def("listRemoteDirectory", &HttpsUploader::listRemoteDirectory,
         py::arg("path"))
    .def("setCustomHeaders", &HttpsUploader::setCustomHeaders,
         py::arg("headers"))
    .def("setEndpointUrl", &HttpsUploader::setEndpointUrl,
         py::arg("url"))
    .def("__repr__", [](const HttpsUploader* uploader) -> std::string {
      return FormatString("HttpsUploader(type='%s')", uploader->type().c_str());
    });
  type_codec->registerStdCodec<std::shared_ptr<HttpsUploader>>(https_uploader_type);

  // AssetUploaderAdapter
  auto asset_uploader_type = py::class_<asset::catalog::AssetUploaderAdapter, asset::catalog::assetuploaderadapter_ptr_t>(module_core, "AssetUploaderAdapter")
    .def("uploadManifest", &asset::catalog::AssetUploaderAdapter::uploadManifest,
         py::arg("manifest"), py::arg("source_dir"))
    .def("uploadAssetFiles", &asset::catalog::AssetUploaderAdapter::uploadAssetFiles,
         py::arg("asset_ids"), py::arg("manifest"), py::arg("source_dir"))
    .def("uploadAssetFile", &asset::catalog::AssetUploaderAdapter::uploadAssetFile,
         py::arg("asset_id"), py::arg("manifest"), py::arg("source_dir"))
    .def("cancel", &asset::catalog::AssetUploaderAdapter::cancel)
    .def("isCancelled", &asset::catalog::AssetUploaderAdapter::isCancelled)
    .def("type", &asset::catalog::AssetUploaderAdapter::type)
    .def("setProgressCallback", [](asset::catalog::assetuploaderadapter_ptr_t adapter, py::function callback) {
      if (!callback.is_none()) {
        asset::catalog::pysafe_upload_progress_callback_t pysafe_cb;
        pysafe_cb._data.makeShared<py::function>(callback);
        pysafe_cb._item = [data = pysafe_cb._data](const asset::catalog::UploadProgress& progress) {
          auto fn = data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(progress);
        };
        adapter->setProgressCallback(pysafe_cb);
      }
    }, py::arg("callback"))
    .def("__repr__", [](asset::catalog::assetuploaderadapter_ptr_t adapter) -> std::string {
      return FormatString("AssetUploaderAdapter(type='%s', cancelled=%s)", 
        adapter->type().c_str(),
        adapter->isCancelled() ? "true" : "false");
    });
  type_codec->registerStdCodec<asset::catalog::assetuploaderadapter_ptr_t>(asset_uploader_type);

  // AssetUploadCoordinator
  auto upload_coordinator_type = py::class_<asset::catalog::AssetUploadCoordinator, asset::catalog::assetuploadcoordinator_ptr_t>(module_core, "AssetUploadCoordinator")
    .def(py::init<>())
    .def("registerUploader", &asset::catalog::AssetUploadCoordinator::registerUploader)
    .def("getUploader", &asset::catalog::AssetUploadCoordinator::getUploader)
    .def("listUploaders", &asset::catalog::AssetUploadCoordinator::listUploaders)
    .def("uploadToMultiple", &asset::catalog::AssetUploadCoordinator::uploadToMultiple,
         py::arg("manifest"), py::arg("source_dir"), py::arg("uploader_names"))
    .def("uploadWithFallback", &asset::catalog::AssetUploadCoordinator::uploadWithFallback,
         py::arg("manifest"), py::arg("source_dir"), py::arg("uploader_names"))
    .def("setAggregateProgressCallback", [](asset::catalog::assetuploadcoordinator_ptr_t coord, py::function callback) {
      if (!callback.is_none()) {
        asset::catalog::pysafe_aggregate_upload_progress_t pysafe_cb;
        pysafe_cb._data.makeShared<py::function>(callback);
        pysafe_cb._item = [data = pysafe_cb._data](const std::string& uploader_name, const asset::catalog::UploadProgress& progress) {
          auto fn = data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(uploader_name, progress);
        };
        coord->setAggregateProgressCallback(pysafe_cb);
      }
    }, py::arg("callback"))
    .def("__repr__", [](asset::catalog::assetuploadcoordinator_ptr_t coord) -> std::string {
      auto uploaders = coord->listUploaders();
      return FormatString("AssetUploadCoordinator(uploaders=%zu)", uploaders.size());
    });
  type_codec->registerStdCodec<asset::catalog::assetuploadcoordinator_ptr_t>(upload_coordinator_type);

  // Factory functions
  module_core.def("createAssetUploader", &asset::catalog::createAssetUploader,
                  py::arg("type"), py::arg("config"),
                  "Create an asset uploader adapter of the specified type");
  
  module_core.def("createAssetUploaderFromUrl", &asset::catalog::createAssetUploaderFromUrl,
                  py::arg("url"), py::arg("config") = nullptr,
                  "Create an asset uploader adapter from a URL (auto-detect type)");
  
  module_core.def("parseUploadUrl", &asset::catalog::parseUploadUrl,
                  py::arg("url"),
                  "Parse an upload URL into a configuration object");
}

} // namespace ork