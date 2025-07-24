////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/URL.h>
#include <ork/util/download.h>
#include <ork/util/download_manager.h>
#include <ork/util/download_group.h>
#include <ork/kernel/opq.h>

namespace ork {

void pyinit_download(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // URL
  /////////////////////////////////////////////////////////////////////////////////
  auto url_type = py::class_<URL, url_ptr_t>(module_core, "URL")
    .def(py::init<>())
    .def(py::init<const std::string&>())
    .def_readwrite("scheme", &URL::_scheme)
    .def_readwrite("host", &URL::_host)
    .def_readwrite("port", &URL::_port)
    .def_readwrite("path", &URL::_path)
    .def_readwrite("query", &URL::_query)
    .def_readwrite("fragment", &URL::_fragment)
    .def_readwrite("userinfo", &URL::_userinfo)
    .def("with_query", &URL::withQuery)
    .def("with_scheme", &URL::withScheme)
    .def("with_host", &URL::withHost)
    .def("with_port", &URL::withPort)
    .def("with_path", &URL::withPath)
    .def("__truediv__", &URL::operator/)
    .def("parent", &URL::parent)
    .def("to_string", &URL::toString)
    .def("is_valid", &URL::isValid)
    .def("is_absolute", &URL::isAbsolute)
    .def_static("encode", &URL::encode)
    .def_static("decode", &URL::decode)
    .def_static("encode_path", &URL::encodePath)
    .def_static("encode_query_value", &URL::encodeQueryValue)
    .def("__str__", &URL::toString)
    .def("__repr__", [](url_ptr_t url) -> std::string {
      return FormatString("URL('%s')", url->toString().c_str());
    });
  type_codec->registerStdCodec<url_ptr_t>(url_type);

  /////////////////////////////////////////////////////////////////////////////////
  // DownloadState enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<DownloadState>(module_core, "DownloadState")
    .value("PENDING", DownloadState::PENDING)
    .value("DOWNLOADING", DownloadState::DOWNLOADING)
    .value("COMPLETED", DownloadState::COMPLETED)
    .value("FAILED", DownloadState::FAILED)
    .value("CANCELLED", DownloadState::CANCELLED);

  /////////////////////////////////////////////////////////////////////////////////
  // Download
  /////////////////////////////////////////////////////////////////////////////////
  auto download_type = py::class_<Download, download_ptr_t>(module_core, "Download")
    .def(py::init<const URL&, const file::Path&>())
    .def_readwrite("url", &Download::_url)
    .def_readwrite("destination_path", &Download::_destination_path)
    .def_readwrite("api_key", &Download::_api_key)
    .def_readwrite("ignore_tls_errors", &Download::_ignore_tls_errors)
    .def_readwrite("headers", &Download::_headers)
    .def_property_readonly("state", [](download_ptr_t dl) { return dl->_state.load(); })
    .def_property_readonly("downloaded_bytes", [](download_ptr_t dl) { return dl->_downloaded_bytes.load(); })
    .def_property_readonly("total_bytes", [](download_ptr_t dl) { return dl->_total_bytes.load(); })
    .def_readonly("error_message", &Download::_error_message)
    .def("set_header", &Download::setHeader)
    .def("set_api_key", &Download::setApiKey)
    .def("progress_percentage", &Download::progressPercentage)
    .def("on_progress", [](download_ptr_t dl, py::function fn) {
      if (!fn.is_none()) {
        dl->_on_progress._data.makeShared<py::function>(fn);
        dl->_on_progress._item = [dl](size_t downloaded, size_t total) {
          auto fn = dl->_on_progress._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(downloaded, total);
        };
      } else {
        dl->_on_progress._item = nullptr;
      }
    })
    .def("on_complete", [](download_ptr_t dl, py::function fn) {
      if (!fn.is_none()) {
        dl->_on_complete._data.makeShared<py::function>(fn);
        dl->_on_complete._item = [dl](bool success, const file::Path& path) {
          auto fn = dl->_on_complete._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(success, path);
        };
      } else {
        dl->_on_complete._item = nullptr;
      }
    })
    .def("on_failure", [](download_ptr_t dl, py::function fn) {
      if (!fn.is_none()) {
        dl->_on_failure._data.makeShared<py::function>(fn);
        dl->_on_failure._item = [dl](const std::string& error) {
          auto fn = dl->_on_failure._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(error);
        };
      } else {
        dl->_on_failure._item = nullptr;
      }
    })
    .def("__repr__", [](download_ptr_t dl) -> std::string {
      return FormatString("Download(url='%s', state=%d, progress=%.1f%%)", 
        dl->_url.toString().c_str(), (int)dl->_state.load(), dl->progressPercentage());
    });
  type_codec->registerStdCodec<download_ptr_t>(download_type);

  /////////////////////////////////////////////////////////////////////////////////
  // DownloadGroup
  /////////////////////////////////////////////////////////////////////////////////
  auto download_group_type = py::class_<DownloadGroup, download_group_ptr_t>(module_core, "DownloadGroup")
    .def(py::init<>())
    .def_readonly("downloads", &DownloadGroup::_downloads)
    .def_property_readonly("completed_count", [](download_group_ptr_t grp) { return grp->_completed_count.load(); })
    .def_property_readonly("failed_count", [](download_group_ptr_t grp) { return grp->_failed_count.load(); })
    .def("add_download", py::overload_cast<const URL&, const file::Path&>(&DownloadGroup::addDownload))
    .def("add_download", py::overload_cast<download_ptr_t>(&DownloadGroup::addDownload))
    .def("is_complete", &DownloadGroup::isComplete)
    .def("all_successful", &DownloadGroup::allSuccessful)
    .def("total_count", &DownloadGroup::totalCount)
    .def("progress_percentage", &DownloadGroup::progressPercentage)
    .def("on_progress", [](download_group_ptr_t grp, py::function fn) {
      if (!fn.is_none()) {
        grp->_on_progress._data.makeShared<py::function>(fn);
        // dont copy fn across lambda captures
        grp->_on_progress._item = [grp](size_t completed, size_t total) {
          auto fn = grp->_on_progress._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(completed, total);
        };
      } else {
        grp->_on_progress._item = nullptr;
      }
    })
    .def("on_complete", [](download_group_ptr_t grp, py::function fn) {
      if (!fn.is_none()) {
        grp->_on_complete._data.makeShared<py::function>(fn);
        grp->_on_complete._item = [grp](bool all_success) {
          auto fn = grp->_on_complete._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(all_success);
        };
      } else {
        grp->_on_complete._item = nullptr;
      }
    })
    .def("on_item_state_change", [](download_group_ptr_t grp, py::function fn) {
      if (!fn.is_none()) {
        grp->_on_item_state_change._data.makeShared<py::function>(fn);
        grp->_on_item_state_change._item = [grp](download_ptr_t dl, DownloadState old_state, DownloadState new_state) {
          auto fn = grp->_on_item_state_change._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(dl, old_state, new_state);
        };
      } else {
        grp->_on_item_state_change._item = nullptr;
      }
    })
    .def("__repr__", [](download_group_ptr_t grp) -> std::string {
      return FormatString("DownloadGroup(total=%zu, completed=%zu, failed=%zu, progress=%.1f%%)", 
        grp->totalCount(), grp->_completed_count.load(), grp->_failed_count.load(), grp->progressPercentage());
    });
  type_codec->registerStdCodec<download_group_ptr_t>(download_group_type);

  /////////////////////////////////////////////////////////////////////////////////
  // DownloadManager
  /////////////////////////////////////////////////////////////////////////////////
  auto download_manager_type = py::class_<DownloadManager, downloadmanager_ptr_t>(module_core, "DownloadManager")
    .def(py::init<>())
    .def(py::init<opq::opq_ptr_t>())
    .def_readwrite("max_concurrent_downloads", &DownloadManager::_max_concurrent_downloads)
    .def("download", &DownloadManager::download)
    .def("downloadGroup", &DownloadManager::downloadGroup)
    .def("set_max_concurrent_downloads", &DownloadManager::setMaxConcurrentDownloads)
    .def("shutdown", &DownloadManager::shutdown)
    .def("is_active", &DownloadManager::isActive)
    .def("active_download_count", &DownloadManager::activeDownloadCount)
    .def("__repr__", [](downloadmanager_ptr_t mgr) -> std::string {
      return FormatString("DownloadManager(max_concurrent=%zu, active=%zu)", 
        mgr->_max_concurrent_downloads, mgr->activeDownloadCount());
    });
  type_codec->registerStdCodec<downloadmanager_ptr_t>(download_manager_type);
}

} // namespace ork