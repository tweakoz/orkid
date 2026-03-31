////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/pack.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/filesystem_model.h>
#include <ork/lev2/ui/filesystem_view.h>
#include <ork/lev2/ui/favorites.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

// Trampoline class for Python subclassing of FilesystemModel
class PyFilesystemModel : public ui::FilesystemModel {
public:
  using ui::FilesystemModel::FilesystemModel;

  std::string getCurrentPath() const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::string,
        ui::FilesystemModel,
        getCurrentPath);
  }

  bool setCurrentPath(const std::string& path) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        bool,
        ui::FilesystemModel,
        setCurrentPath,
        path);
  }

  std::string getParentPath() const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::string,
        ui::FilesystemModel,
        getParentPath);
  }

  bool exists(const std::string& path) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        bool,
        ui::FilesystemModel,
        exists,
        path);
  }

  bool isDirectory(const std::string& path) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        bool,
        ui::FilesystemModel,
        isDirectory,
        path);
  }

  ui::filesystem_entry_list_t getEntries() const override {
    py::gil_scoped_acquire acquire;
    // Call Python method and convert result
    py::object py_result = py::cast(this).attr("getEntries")();
    ui::filesystem_entry_list_t entries;
    if (!py_result.is_none() && py::isinstance<py::list>(py_result)) {
      for (auto item : py_result.cast<py::list>()) {
        if (py::isinstance<py::dict>(item)) {
          auto d = item.cast<py::dict>();
          ui::FilesystemEntry entry;
          if (d.contains("path")) entry.path = d["path"].cast<std::string>();
          if (d.contains("name")) entry.name = d["name"].cast<std::string>();
          if (d.contains("type")) {
            std::string type_str = d["type"].cast<std::string>();
            if (type_str == "file") entry.type = ui::FileType::File;
            else if (type_str == "directory") entry.type = ui::FileType::Directory;
            else if (type_str == "symlink") entry.type = ui::FileType::Symlink;
            else entry.type = ui::FileType::Unknown;
          }
          if (d.contains("size")) entry.size = d["size"].cast<size_t>();
          if (d.contains("modified_time")) entry.modified_time = d["modified_time"].cast<time_t>();
          if (d.contains("mime_type")) entry.mime_type = d["mime_type"].cast<std::string>();
          if (d.contains("extension")) entry.extension = d["extension"].cast<std::string>();
          if (d.contains("is_hidden")) entry.is_hidden = d["is_hidden"].cast<bool>();
          if (d.contains("is_readable")) entry.is_readable = d["is_readable"].cast<bool>();
          if (d.contains("is_writable")) entry.is_writable = d["is_writable"].cast<bool>();
          if (d.contains("description")) entry.description = d["description"].cast<std::string>();
          if (d.contains("options")) entry.options = d["options"].cast<std::string>();
          entries.push_back(entry);
        }
      }
    }
    return entries;
  }

  ui::FilesystemEntry getEntry(const std::string& path) const override {
    py::gil_scoped_acquire acquire;
    py::object py_result = py::cast(this).attr("getEntry")(path);
    ui::FilesystemEntry entry;
    entry.path = path;
    if (!py_result.is_none() && py::isinstance<py::dict>(py_result)) {
      auto d = py_result.cast<py::dict>();
      if (d.contains("name")) entry.name = d["name"].cast<std::string>();
      if (d.contains("type")) {
        std::string type_str = d["type"].cast<std::string>();
        if (type_str == "file") entry.type = ui::FileType::File;
        else if (type_str == "directory") entry.type = ui::FileType::Directory;
        else if (type_str == "symlink") entry.type = ui::FileType::Symlink;
        else entry.type = ui::FileType::Unknown;
      }
      if (d.contains("size")) entry.size = d["size"].cast<size_t>();
      if (d.contains("modified_time")) entry.modified_time = d["modified_time"].cast<time_t>();
      if (d.contains("mime_type")) entry.mime_type = d["mime_type"].cast<std::string>();
      if (d.contains("extension")) entry.extension = d["extension"].cast<std::string>();
      if (d.contains("is_hidden")) entry.is_hidden = d["is_hidden"].cast<bool>();
      if (d.contains("is_readable")) entry.is_readable = d["is_readable"].cast<bool>();
      if (d.contains("is_writable")) entry.is_writable = d["is_writable"].cast<bool>();
      if (d.contains("description")) entry.description = d["description"].cast<std::string>();
      if (d.contains("options")) entry.options = d["options"].cast<std::string>();
    }
    return entry;
  }

  std::string getDisplayName(const std::string& path) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::string,
        ui::FilesystemModel,
        getDisplayName,
        path);
  }

  bool isReadOnly() const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        bool,
        ui::FilesystemModel,
        isReadOnly);
  }

  bool createDirectory(const std::string& name) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        bool,
        ui::FilesystemModel,
        createDirectory,
        name);
  }

  bool deleteItem(const std::string& path) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        bool,
        ui::FilesystemModel,
        deleteItem,
        path);
  }

  std::string renameItem(const std::string& path, const std::string& new_name) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        std::string,
        ui::FilesystemModel,
        renameItem,
        path, new_name);
  }

  bool copyItem(const std::string& src_path, const std::string& dest_path) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        bool,
        ui::FilesystemModel,
        copyItem,
        src_path, dest_path);
  }

  bool moveItem(const std::string& src_path, const std::string& dest_path) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        bool,
        ui::FilesystemModel,
        moveItem,
        src_path, dest_path);
  }

  image_ptr_t getIcon(const std::string& path, int size) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        image_ptr_t,
        ui::FilesystemModel,
        getIcon,
        path, size);
  }

  image_list_t getIconSequence(const std::string& path, int size) override {
    py::gil_scoped_acquire acquire;
    py::object py_result = py::cast(this).attr("getIconSequence")(path, size);
    if (py_result.is_none()) {
      return {};
    }
    if (py::isinstance<py::list>(py_result)) {
      image_list_t result;
      for (auto item : py_result.cast<py::list>()) {
        if (!item.is_none()) {
          result.push_back(item.cast<image_ptr_t>());
        }
      }
      return result;
    }
    return {};
  }

  image_provider_ptr_t getIconProvider(const std::string& path, int size) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        image_provider_ptr_t,
        ui::FilesystemModel,
        getIconProvider,
        path, size);
  }

  image_provider_ptr_t getThumbnailProvider(const std::string& path, int size) override {
    py::gil_scoped_acquire acquire;
    py::object py_result = py::cast(this).attr("getThumbnailProvider")(path, size);
    if (py_result.is_none()) {
      return nullptr;
    }
    // If Python returns a callable, wrap it in an ImageProvider
    if (py::isinstance<py::function>(py_result)) {
      auto provider = std::make_shared<ImageProvider>();
      py::function py_func = py_result.cast<py::function>();
      provider->_func = [py_func]() -> image_ptr_t {
        py::gil_scoped_acquire acquire;
        py::object result = py_func();
        if (result.is_none()) {
          return nullptr;
        }
        return result.cast<image_ptr_t>();
      };
      return provider;
    }
    // If Python returns an ImageProvider directly
    if (py::isinstance<image_provider_ptr_t>(py_result)) {
      return py_result.cast<image_provider_ptr_t>();
    }
    return nullptr;
  }

  bool hasThumbnail(const std::string& path) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        bool,
        ui::FilesystemModel,
        hasThumbnail,
        path);
  }

  std::string modelIdentifier() const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::string,
        ui::FilesystemModel,
        modelIdentifier);
  }
};

void pyinit_ui_filesystem(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // FileType enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<ui::FileType>(uimodule, "FileType")
      .value("Unknown", ui::FileType::Unknown)
      .value("File", ui::FileType::File)
      .value("Directory", ui::FileType::Directory)
      .value("Symlink", ui::FileType::Symlink);

  /////////////////////////////////////////////////////////////////////////////////
  // FilesystemEntry struct
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ui::FilesystemEntry>(uimodule, "FilesystemEntry")
      .def(py::init<>())
      .def_readwrite("path", &ui::FilesystemEntry::path)
      .def_readwrite("name", &ui::FilesystemEntry::name)
      .def_readwrite("type", &ui::FilesystemEntry::type)
      .def_readwrite("size", &ui::FilesystemEntry::size)
      .def_readwrite("modified_time", &ui::FilesystemEntry::modified_time)
      .def_readwrite("mime_type", &ui::FilesystemEntry::mime_type)
      .def_readwrite("extension", &ui::FilesystemEntry::extension)
      .def_readwrite("is_hidden", &ui::FilesystemEntry::is_hidden)
      .def_readwrite("is_readable", &ui::FilesystemEntry::is_readable)
      .def_readwrite("is_writable", &ui::FilesystemEntry::is_writable)
      .def_readwrite("description", &ui::FilesystemEntry::description)
      .def_readwrite("options", &ui::FilesystemEntry::options)
      .def("__repr__", [](const ui::FilesystemEntry& e) {
        const char* type_str = "unknown";
        switch (e.type) {
          case ui::FileType::File: type_str = "file"; break;
          case ui::FileType::Directory: type_str = "directory"; break;
          case ui::FileType::Symlink: type_str = "symlink"; break;
          default: break;
        }
        return FormatString("<FilesystemEntry name<%s> type<%s>>", e.name.c_str(), type_str);
      });

  /////////////////////////////////////////////////////////////////////////////////
  // SortField and SortOrder enums
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<ui::FilesystemModel::SortField>(uimodule, "FilesystemSortField")
      .value("Name", ui::FilesystemModel::SortField::Name)
      .value("Size", ui::FilesystemModel::SortField::Size)
      .value("Type", ui::FilesystemModel::SortField::Type)
      .value("ModifiedTime", ui::FilesystemModel::SortField::ModifiedTime);

  py::enum_<ui::FilesystemModel::SortOrder>(uimodule, "FilesystemSortOrder")
      .value("Ascending", ui::FilesystemModel::SortOrder::Ascending)
      .value("Descending", ui::FilesystemModel::SortOrder::Descending);

  /////////////////////////////////////////////////////////////////////////////////
  // FilesystemViewMode enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<ui::FilesystemViewMode>(uimodule, "FilesystemViewMode")
      .value("List", ui::FilesystemViewMode::List)
      .value("Icon", ui::FilesystemViewMode::Icon);

  /////////////////////////////////////////////////////////////////////////////////
  // FilesystemModel base class (can be subclassed in Python)
  /////////////////////////////////////////////////////////////////////////////////
  auto filesystem_model_type = //
      py::class_<ui::FilesystemModel, PyFilesystemModel, ui::filesystem_model_ptr_t>(uimodule, "FilesystemModel")
          .def(py::init<>())
          .def("getCurrentPath", &ui::FilesystemModel::getCurrentPath)
          .def("setCurrentPath", &ui::FilesystemModel::setCurrentPath)
          .def("getParentPath", &ui::FilesystemModel::getParentPath)
          .def("exists", &ui::FilesystemModel::exists)
          .def("isDirectory", &ui::FilesystemModel::isDirectory)
          .def(
              "getEntries",
              [](ui::filesystem_model_ptr_t model) -> py::list {
                auto entries = model->getEntries();
                py::list result;
                for (const auto& e : entries) {
                  py::dict d;
                  d["path"] = e.path;
                  d["name"] = e.name;
                  switch (e.type) {
                    case ui::FileType::File: d["type"] = "file"; break;
                    case ui::FileType::Directory: d["type"] = "directory"; break;
                    case ui::FileType::Symlink: d["type"] = "symlink"; break;
                    default: d["type"] = "unknown"; break;
                  }
                  d["size"] = e.size;
                  d["modified_time"] = e.modified_time;
                  d["mime_type"] = e.mime_type;
                  d["extension"] = e.extension;
                  d["is_hidden"] = e.is_hidden;
                  d["is_readable"] = e.is_readable;
                  d["is_writable"] = e.is_writable;
                  d["description"] = e.description;
                  d["options"] = e.options;
                  result.append(d);
                }
                return result;
              })
          .def(
              "getEntry",
              [](ui::filesystem_model_ptr_t model, const std::string& path) -> py::dict {
                auto e = model->getEntry(path);
                py::dict d;
                d["path"] = e.path;
                d["name"] = e.name;
                switch (e.type) {
                  case ui::FileType::File: d["type"] = "file"; break;
                  case ui::FileType::Directory: d["type"] = "directory"; break;
                  case ui::FileType::Symlink: d["type"] = "symlink"; break;
                  default: d["type"] = "unknown"; break;
                }
                d["size"] = e.size;
                d["modified_time"] = e.modified_time;
                d["mime_type"] = e.mime_type;
                d["extension"] = e.extension;
                d["is_hidden"] = e.is_hidden;
                d["is_readable"] = e.is_readable;
                d["is_writable"] = e.is_writable;
                d["description"] = e.description;
                d["options"] = e.options;
                return d;
              })
          .def("getDisplayName", &ui::FilesystemModel::getDisplayName)
          .def_property(
              "filter",
              &ui::FilesystemModel::getFilter,
              &ui::FilesystemModel::setFilter)
          .def_property(
              "name_filter",
              &ui::FilesystemModel::getNameFilter,
              &ui::FilesystemModel::setNameFilter)
          .def_property(
              "show_hidden",
              &ui::FilesystemModel::getShowHidden,
              &ui::FilesystemModel::setShowHidden)
          .def_property(
              "show_directories",
              &ui::FilesystemModel::getShowDirectories,
              &ui::FilesystemModel::setShowDirectories)
          .def("isReadOnly", &ui::FilesystemModel::isReadOnly)
          .def("createDirectory", &ui::FilesystemModel::createDirectory)
          .def("deleteItem", &ui::FilesystemModel::deleteItem)
          .def("renameItem", &ui::FilesystemModel::renameItem)
          .def("copyItem", &ui::FilesystemModel::copyItem)
          .def("moveItem", &ui::FilesystemModel::moveItem)
          .def(
              "getIcon",
              [](ui::filesystem_model_ptr_t model, const std::string& path, int size) -> image_ptr_t {
                return model->getIcon(path, size);
              },
              py::arg("path"),
              py::arg("size") = 64,
              "Get icon image for a path (returns None to use default)")
          .def(
              "getIconSequence",
              [](ui::filesystem_model_ptr_t model, const std::string& path, int size) -> py::list {
                auto images = model->getIconSequence(path, size);
                py::list result;
                for (auto& img : images) {
                  result.append(img);
                }
                return result;
              },
              py::arg("path"),
              py::arg("size") = 64,
              "Get animated icon sequence for a path (returns empty list to use getIcon)")
          .def(
              "getIconProvider",
              [](ui::filesystem_model_ptr_t model, const std::string& path, int size) -> image_provider_ptr_t {
                return model->getIconProvider(path, size);
              },
              py::arg("path"),
              py::arg("size") = 64,
              "Get icon provider for lazy loading (returns None to use getIcon or default)")
          .def(
              "getThumbnailProvider",
              [](ui::filesystem_model_ptr_t model, const std::string& path, int size) -> image_provider_ptr_t {
                return model->getThumbnailProvider(path, size);
              },
              py::arg("path"),
              py::arg("size") = 64)
          .def("hasThumbnail", &ui::FilesystemModel::hasThumbnail)
          .def("modelIdentifier", &ui::FilesystemModel::modelIdentifier)
          .def_property(
              "sort_field",
              &ui::FilesystemModel::getSortField,
              &ui::FilesystemModel::setSortField)
          .def_property(
              "sort_order",
              &ui::FilesystemModel::getSortOrder,
              &ui::FilesystemModel::setSortOrder)
          .def_property(
              "directories_first",
              &ui::FilesystemModel::getDirectoriesFirst,
              &ui::FilesystemModel::setDirectoriesFirst)
          .def("notifyModelChanged", &ui::FilesystemModel::notifyModelChanged)
          .def("notifyDirectoryChanged", &ui::FilesystemModel::notifyDirectoryChanged)
          .def("__repr__", [](ui::filesystem_model_ptr_t model) {
            return FormatString("<FilesystemModel %p>", (void*)model.get());
          });

  type_codec->registerStdCodec<ui::filesystem_model_ptr_t>(filesystem_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // LocalFilesystemModel - concrete model for real filesystem
  /////////////////////////////////////////////////////////////////////////////////
  auto local_filesystem_model_type = //
      py::class_<ui::LocalFilesystemModel, ui::FilesystemModel, ui::local_filesystem_model_ptr_t>(uimodule, "LocalFilesystemModel")
          .def(py::init<>())
          .def(py::init<const std::string&>())
          .def_property(
              "read_only",
              &ui::LocalFilesystemModel::isReadOnly,
              &ui::LocalFilesystemModel::setReadOnly)
          .def_property(
              "root_path",
              &ui::LocalFilesystemModel::getRootPath,
              &ui::LocalFilesystemModel::setRootPath)
          .def("__repr__", [](ui::local_filesystem_model_ptr_t model) {
            return FormatString("<LocalFilesystemModel path<%s>>", model->getCurrentPath().c_str());
          });

  type_codec->registerStdCodec<ui::local_filesystem_model_ptr_t>(local_filesystem_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // FavoriteEntry - a favorite location with associated view state
  // NOTE: Must be registered before FilesystemView which uses it
  /////////////////////////////////////////////////////////////////////////////////
  auto favorite_entry_type = //
      py::class_<ui::FavoriteEntry, ui::favorite_entry_ptr_t>(uimodule, "FavoriteEntry")
          .def(py::init<>())
          .def_readwrite("uuid", &ui::FavoriteEntry::uuid,
              "Unique identifier")
          .def_readwrite("path", &ui::FavoriteEntry::path,
              "Directory path")
          .def_readwrite("name", &ui::FavoriteEntry::name,
              "Display name (defaults to path basename)")
          .def_readwrite("name_filter", &ui::FavoriteEntry::name_filter,
              "Name filter pattern")
          .def_readwrite("sort_field", &ui::FavoriteEntry::sort_field,
              "Sort field")
          .def_readwrite("sort_order", &ui::FavoriteEntry::sort_order,
              "Sort order")
          .def_readwrite("directories_first", &ui::FavoriteEntry::directories_first,
              "Show directories first")
          .def_readwrite("show_hidden", &ui::FavoriteEntry::show_hidden,
              "Show hidden files")
          .def_static("fromModel", &ui::FavoriteEntry::fromModel,
              py::arg("model"), py::arg("display_name") = "",
              "Create a FavoriteEntry from current model state")
          .def("applyToModel", &ui::FavoriteEntry::applyToModel,
              py::arg("model"),
              "Apply this favorite's state to a model")
          .def("displayName", &ui::FavoriteEntry::displayName,
              "Get display name (returns name if set, otherwise path basename)")
          .def("__repr__", [](ui::favorite_entry_ptr_t entry) {
            return FormatString("<FavoriteEntry uuid<%s> path<%s> name<%s>>",
                entry->uuid.c_str(), entry->path.c_str(), entry->displayName().c_str());
          });

  type_codec->registerStdCodec<ui::favorite_entry_ptr_t>(favorite_entry_type);

  /////////////////////////////////////////////////////////////////////////////////
  // FavoritesManager - manages favorite paths for filesystem models
  // NOTE: Must be registered before FilesystemView which uses it
  /////////////////////////////////////////////////////////////////////////////////
  auto favorites_manager_type = //
      py::class_<ui::FavoritesManager, ui::favorites_manager_ptr_t>(uimodule, "FavoritesManager")
          .def_static("instance", &ui::FavoritesManager::instance,
              "Get the singleton FavoritesManager instance")
          // Full-state favorites (with filter/sort)
          .def("addFavoriteEntry", &ui::FavoritesManager::addFavoriteEntry,
              py::arg("model_id"), py::arg("entry"),
              "Add a favorite entry for a model")
          .def("removeFavoriteEntry", &ui::FavoritesManager::removeFavoriteEntry,
              py::arg("model_id"), py::arg("uuid"),
              "Remove a favorite entry by UUID")
          .def("getFavoriteEntry", &ui::FavoritesManager::getFavoriteEntry,
              py::arg("model_id"), py::arg("uuid"),
              "Get a favorite entry by UUID")
          .def("updateFavoriteEntry", &ui::FavoritesManager::updateFavoriteEntry,
              py::arg("model_id"), py::arg("entry"),
              "Update an existing favorite entry (matched by UUID)")
          .def("getFavoriteEntries", &ui::FavoritesManager::getFavoriteEntries,
              py::arg("model_id"),
              "Get all favorite entries for a model")
          // Simple favorites (path only, backwards compatible)
          .def("addFavorite", &ui::FavoritesManager::addFavorite,
              py::arg("model_id"), py::arg("path"),
              "Add a favorite path for a model")
          .def("removeFavorite", &ui::FavoritesManager::removeFavorite,
              py::arg("model_id"), py::arg("path"),
              "Remove a favorite path for a model")
          .def("isFavorite", &ui::FavoritesManager::isFavorite,
              py::arg("model_id"), py::arg("path"),
              "Check if a path is a favorite for a model")
          .def("getFavorites", &ui::FavoritesManager::getFavorites,
              py::arg("model_id"),
              "Get all favorites for a model (paths only)")
          .def("clearFavorites", &ui::FavoritesManager::clearFavorites,
              py::arg("model_id"),
              "Clear all favorites for a model")
          .def("moveFavoriteUp", &ui::FavoritesManager::moveFavoriteUp,
              py::arg("model_id"), py::arg("path"),
              "Move a favorite up in the list")
          .def("moveFavoriteDown", &ui::FavoritesManager::moveFavoriteDown,
              py::arg("model_id"), py::arg("path"),
              "Move a favorite down in the list")
          // Recent paths
          .def("addRecent", &ui::FavoritesManager::addRecent,
              py::arg("model_id"), py::arg("path"),
              "Add a path to recent paths for a model")
          .def("getRecent", &ui::FavoritesManager::getRecent,
              py::arg("model_id"),
              "Get recent paths for a model")
          .def("clearRecent", &ui::FavoritesManager::clearRecent,
              py::arg("model_id"),
              "Clear recent paths for a model")
          .def_property("max_recent",
              &ui::FavoritesManager::getMaxRecent,
              &ui::FavoritesManager::setMaxRecent,
              "Maximum number of recent paths to remember")
          // Persistence
          .def("save", &ui::FavoritesManager::save,
              "Force save to disk")
          // Callbacks
          .def("onFavoritesChanged",
              [](ui::favorites_manager_ptr_t mgr, py::object callback) {
                mgr->_onFavoritesChanged = [callback](const std::string& model_id) {
                  py::gil_scoped_acquire acquire;
                  callback(model_id);
                };
              },
              "Set callback for when favorites change")
          .def("onRecentChanged",
              [](ui::favorites_manager_ptr_t mgr, py::object callback) {
                mgr->_onRecentChanged = [callback](const std::string& model_id) {
                  py::gil_scoped_acquire acquire;
                  callback(model_id);
                };
              },
              "Set callback for when recent paths change")
          .def("__repr__", [](ui::favorites_manager_ptr_t mgr) {
            return FormatString("<FavoritesManager>");
          });

  type_codec->registerStdCodec<ui::favorites_manager_ptr_t>(favorites_manager_type);

  /////////////////////////////////////////////////////////////////////////////////
  // FilesystemView widget
  /////////////////////////////////////////////////////////////////////////////////
  auto filesystem_view_type = //
      py::class_<ui::FilesystemView, ui::Group, ui::filesystem_view_ptr_t>(uimodule, "FilesystemView")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::filesystem_view_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto view         = std::make_shared<ui::FilesystemView>(name);
                return view;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::FilesystemView>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "model",
              [](ui::filesystem_view_ptr_t view) -> ui::filesystem_model_ptr_t { //
                return view->getModel();
              },
              [](ui::filesystem_view_ptr_t view, ui::filesystem_model_ptr_t model) { //
                view->setModel(model);
              })
          .def_property(
              "view_mode",
              &ui::FilesystemView::getViewMode,
              &ui::FilesystemView::setViewMode)
          .def_property(
              "selected_path",
              &ui::FilesystemView::getSelectedPath,
              &ui::FilesystemView::setSelectedPath)
          .def_property(
              "selected_paths",
              [](ui::filesystem_view_ptr_t view) -> py::list { //
                py::list result;
                for (const auto& path : view->getSelectedPaths()) {
                  result.append(path);
                }
                return result;
              },
              [](ui::filesystem_view_ptr_t view, py::list paths) { //
                view->clearSelection();
                for (auto path : paths) {
                  view->addToSelection(path.cast<std::string>());
                }
              })
          .def("addToSelection", &ui::FilesystemView::addToSelection)
          .def("removeFromSelection", &ui::FilesystemView::removeFromSelection)
          .def("clearSelection", &ui::FilesystemView::clearSelection)
          .def("selectAll", &ui::FilesystemView::selectAll)
          .def("isSelected", &ui::FilesystemView::isSelected)
          .def_property(
              "allow_multiselect",
              &ui::FilesystemView::getAllowMultiSelect,
              &ui::FilesystemView::setAllowMultiSelect)
          .def("navigateTo", &ui::FilesystemView::navigateTo)
          .def("navigateUp", &ui::FilesystemView::navigateUp)
          .def("activateItem", &ui::FilesystemView::activateItem)
          .def("refresh", &ui::FilesystemView::refresh)
          // Favorites management
          .def("addCurrentAsFavorite", &ui::FilesystemView::addCurrentAsFavorite,
              py::arg("display_name") = "",
              "Add current location + view state as a favorite")
          .def("removeCurrentFromFavorites", &ui::FilesystemView::removeCurrentFromFavorites,
              "Remove current path from favorites")
          .def("isCurrentFavorite", &ui::FilesystemView::isCurrentFavorite,
              "Check if current path is a favorite")
          .def("applyFavorite", &ui::FilesystemView::applyFavorite,
              py::arg("entry"),
              "Apply a favorite entry (navigate and restore view state)")
          .def("getCurrentAsFavoriteEntry", &ui::FilesystemView::getCurrentAsFavoriteEntry,
              py::arg("display_name") = "",
              "Get current state as a FavoriteEntry (without adding to favorites)")
          // Inline editing
          .def("startEditing", &ui::FilesystemView::startEditing)
          .def("cancelEditing", &ui::FilesystemView::cancelEditing)
          .def("commitEditing", &ui::FilesystemView::commitEditing)
          .def("isEditing", &ui::FilesystemView::isEditing)
          // Callbacks
          .def(
              "onSelect",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onSelect = [callback](const std::string& path) {
                  py::gil_scoped_acquire acquire;
                  callback(path);
                };
              })
          .def(
              "onActivate",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onActivate = [callback](const std::string& path) {
                  py::gil_scoped_acquire acquire;
                  callback(path);
                };
              })
          .def(
              "onDirectoryChanged",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onDirectoryChanged = [callback](const std::string& path) {
                  py::gil_scoped_acquire acquire;
                  callback(path);
                };
              })
          .def(
              "onDelete",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onDelete = [callback](const std::string& path) {
                  py::gil_scoped_acquire acquire;
                  callback(path);
                };
              })
          .def(
              "onRename",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onRename = [callback](const std::string& old_path, const std::string& new_name) {
                  py::gil_scoped_acquire acquire;
                  callback(old_path, new_name);
                };
              })
          .def(
              "onContextMenu",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onContextMenu = [callback](const std::string& path, int x, int y) {
                  py::gil_scoped_acquire acquire;
                  callback(path, x, y);
                };
              })
          .def(
              "onHover",
              [](ui::filesystem_view_ptr_t view, py::object callback) { //
                view->_onHover = [callback](const std::string& path) {
                  py::gil_scoped_acquire acquire;
                  callback(path);
                };
              })
          // Appearance - List mode
          .def_readwrite("item_height", &ui::FilesystemView::_item_height)
          .def_readwrite("icon_column_width", &ui::FilesystemView::_icon_column_width)
          .def_readwrite("name_column_width", &ui::FilesystemView::_name_column_width)
          .def_readwrite("size_column_width", &ui::FilesystemView::_size_column_width)
          .def_readwrite("type_column_width", &ui::FilesystemView::_type_column_width)
          .def_readwrite("date_column_width", &ui::FilesystemView::_date_column_width)
          .def_readwrite("description_column_width", &ui::FilesystemView::_description_column_width)
          .def_readwrite("show_description_column", &ui::FilesystemView::_show_description_column)
          .def_readwrite("options_column_width", &ui::FilesystemView::_options_column_width)
          .def_readwrite("show_options_column", &ui::FilesystemView::_show_options_column)
          .def_readwrite("show_size_column", &ui::FilesystemView::_show_size_column)
          .def_readwrite("show_type_column", &ui::FilesystemView::_show_type_column)
          .def_readwrite("show_date_column", &ui::FilesystemView::_show_date_column)
          // Appearance - Icon mode
          .def_readwrite("icon_size", &ui::FilesystemView::_icon_size)
          .def_readwrite("icon_spacing", &ui::FilesystemView::_icon_spacing)
          .def_readwrite("icon_label_height", &ui::FilesystemView::_icon_label_height)
          .def_readwrite("icon_center_h", &ui::FilesystemView::_icon_center_h)
          .def_readwrite("icon_center_v", &ui::FilesystemView::_icon_center_v)
          .def_readwrite("icon_anim_fps", &ui::FilesystemView::_icon_anim_fps)
          // Appearance - Common
          .def_property(
              "bgcolor",
              [](ui::filesystem_view_ptr_t view) -> fvec4 { //
                return view->_bgcolor;
              },
              [](ui::filesystem_view_ptr_t view, fvec4 c) { //
                view->_bgcolor = c;
              })
          .def_property(
              "text_color",
              [](ui::filesystem_view_ptr_t view) -> fvec4 { //
                return view->_text_color;
              },
              [](ui::filesystem_view_ptr_t view, fvec4 c) { //
                view->_text_color = c;
              })
          .def_property(
              "selected_color",
              [](ui::filesystem_view_ptr_t view) -> fvec4 { //
                return view->_selected_color;
              },
              [](ui::filesystem_view_ptr_t view, fvec4 c) { //
                view->_selected_color = c;
              })
          .def_property(
              "hover_color",
              [](ui::filesystem_view_ptr_t view) -> fvec4 { //
                return view->_hover_color;
              },
              [](ui::filesystem_view_ptr_t view, fvec4 c) { //
                view->_hover_color = c;
              })
          .def_property(
              "directory_color",
              [](ui::filesystem_view_ptr_t view) -> fvec4 { //
                return view->_directory_color;
              },
              [](ui::filesystem_view_ptr_t view, fvec4 c) { //
                view->_directory_color = c;
              })
          .def_property(
              "header_bgcolor",
              [](ui::filesystem_view_ptr_t view) -> fvec4 { //
                return view->_header_bgcolor;
              },
              [](ui::filesystem_view_ptr_t view, fvec4 c) { //
                view->_header_bgcolor = c;
              })
          .def_readwrite("draw_background", &ui::FilesystemView::_draw_background)
          .def_property(
              "draw_header",
              [](ui::filesystem_view_ptr_t view) -> bool { return view->_draw_header; },
              [](ui::filesystem_view_ptr_t view, bool val) {
                view->_draw_header = val;
                if (view->_header_widget) view->_header_widget->_enable = val;
              })
          .def_property(
              "draw_path_bar",
              [](ui::filesystem_view_ptr_t view) -> bool { return view->_draw_path_bar; },
              [](ui::filesystem_view_ptr_t view, bool val) {
                view->_draw_path_bar = val;
                if (view->_path_bar_widget) view->_path_bar_widget->_enable = val;
              })
          .def_property(
              "font",
              [](ui::filesystem_view_ptr_t view) -> font_ptr_t { //
                return view->_font;
              },
              [](ui::filesystem_view_ptr_t view, font_ptr_t font) { //
                view->_font = font;
              })
          .def_property(
              "small_font",
              [](ui::filesystem_view_ptr_t view) -> font_ptr_t { //
                return view->_small_font;
              },
              [](ui::filesystem_view_ptr_t view, font_ptr_t font) { //
                view->_small_font = font;
              })
          .def(
              "setFontSize",
              [](ui::filesystem_view_ptr_t view, int size) { view->setFontSize(size); },
              "Set font size (e.g. 16 for i16). Small font is automatically size-2.")
          .def_property(
              "folder_icon",
              &ui::FilesystemView::getFolderIcon,
              &ui::FilesystemView::setFolderIcon,
              "Default folder icon image (converted to texture lazily)")
          .def_property(
              "file_icon",
              &ui::FilesystemView::getFileIcon,
              &ui::FilesystemView::setFileIcon,
              "Default file icon image (converted to texture lazily)")
          .def("clearIconCache", &ui::FilesystemView::clearIconCache,
              "Clear the icon cache (useful after directory change)")
          .def("clearIconCacheForPath", &ui::FilesystemView::clearIconCacheForPath,
              py::arg("path"),
              "Clear cached icon for a specific path only")
          .def(
              "addToolbar",
              [](ui::filesystem_view_ptr_t view, const std::string& name, int height) -> ui::toolbar_ptr_t {
                return view->addToolbar(name, height);
              },
              py::arg("name"),
              py::arg("height") = 24,
              "Add a toolbar between header and content. Returns the Toolbar widget.")
          .def(
              "removeToolbar",
              [](ui::filesystem_view_ptr_t view, ui::toolbar_ptr_t toolbar) {
                view->removeToolbar(toolbar);
              },
              py::arg("toolbar"),
              "Remove a toolbar previously added with addToolbar()")
          .def("__repr__", [](ui::filesystem_view_ptr_t view) {
            return FormatString("<FilesystemView name<%s> widget<%p>>", view->GetName().c_str(), (void*)view.get());
          });

  type_codec->registerStdCodec<ui::filesystem_view_ptr_t>(filesystem_view_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
