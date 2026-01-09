////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/ui/widget.h>
#include <ork/lev2/ui/group.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/ui/filesystem_model.h>
#include <ork/lev2/ui/filesystem_view.h>

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
                return d;
              })
          .def("getDisplayName", &ui::FilesystemModel::getDisplayName)
          .def_property(
              "filter",
              &ui::FilesystemModel::getFilter,
              &ui::FilesystemModel::setFilter)
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
              "getThumbnailProvider",
              [](ui::filesystem_model_ptr_t model, const std::string& path, int size) -> image_provider_ptr_t {
                return model->getThumbnailProvider(path, size);
              },
              py::arg("path"),
              py::arg("size") = 64)
          .def("hasThumbnail", &ui::FilesystemModel::hasThumbnail)
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
  // FilesystemView widget
  /////////////////////////////////////////////////////////////////////////////////
  auto filesystem_view_type = //
      py::class_<ui::FilesystemView, ui::Widget, ui::filesystem_view_ptr_t>(uimodule, "FilesystemView")
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
          // Appearance - List mode
          .def_readwrite("item_height", &ui::FilesystemView::_item_height)
          .def_readwrite("icon_column_width", &ui::FilesystemView::_icon_column_width)
          .def_readwrite("name_column_width", &ui::FilesystemView::_name_column_width)
          .def_readwrite("size_column_width", &ui::FilesystemView::_size_column_width)
          .def_readwrite("type_column_width", &ui::FilesystemView::_type_column_width)
          .def_readwrite("date_column_width", &ui::FilesystemView::_date_column_width)
          .def_readwrite("show_size_column", &ui::FilesystemView::_show_size_column)
          .def_readwrite("show_type_column", &ui::FilesystemView::_show_type_column)
          .def_readwrite("show_date_column", &ui::FilesystemView::_show_date_column)
          // Appearance - Icon mode
          .def_readwrite("icon_size", &ui::FilesystemView::_icon_size)
          .def_readwrite("icon_spacing", &ui::FilesystemView::_icon_spacing)
          .def_readwrite("icon_label_height", &ui::FilesystemView::_icon_label_height)
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
          .def_readwrite("draw_header", &ui::FilesystemView::_draw_header)
          .def_readwrite("draw_path_bar", &ui::FilesystemView::_draw_path_bar)
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
          .def("__repr__", [](ui::filesystem_view_ptr_t view) {
            return FormatString("<FilesystemView name<%s> widget<%p>>", view->GetName().c_str(), (void*)view.get());
          });

  type_codec->registerStdCodec<ui::filesystem_view_ptr_t>(filesystem_view_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
