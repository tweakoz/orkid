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
#include <ork/lev2/ui/outliner.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

// Trampoline class for Python subclassing of OutlinerModel
class PyOutlinerModel : public ui::OutlinerModel {
public:
  using ui::OutlinerModel::OutlinerModel;

  std::vector<std::string> getChildren(const std::string& parent_key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::vector<std::string>,
        ui::OutlinerModel,
        getChildren,
        parent_key);
  }

  std::string getDisplayName(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::string,
        ui::OutlinerModel,
        getDisplayName,
        key);
  }

  bool hasChildren(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        bool,
        ui::OutlinerModel,
        hasChildren,
        key);
  }

  // getValue returns py::object in Python, we convert via codec
  svar128_t getValue(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    // Call Python method and get py::object result
    py::object py_result = py::cast(this).attr("getValue")(key);
    // For now, return empty if Python returns None
    if (py_result.is_none()) {
      return svar128_t();
    }
    // Convert via type codec
    auto type_codec = python::pb11_typecodec_t::instance();
    return type_codec->decode(py_result);
  }

  void addItem(const std::string& parent_key, const std::string& name, svar128_t value) override {
    py::gil_scoped_acquire acquire;
    // Convert value to py::object via codec
    auto type_codec = python::pb11_typecodec_t::instance();
    py::object py_value = type_codec->encode(value);
    py::cast(this).attr("addItem")(parent_key, name, py_value);
  }

  void removeItem(const std::string& key) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        void,
        ui::OutlinerModel,
        removeItem,
        key);
  }

  void moveItem(const std::string& key, const std::string& new_parent_key) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        void,
        ui::OutlinerModel,
        moveItem,
        key, new_parent_key);
  }

  void updateItem(const std::string& key, svar128_t value) override {
    py::gil_scoped_acquire acquire;
    // Convert value to py::object via codec
    auto type_codec = python::pb11_typecodec_t::instance();
    py::object py_value = type_codec->encode(value);
    py::cast(this).attr("updateItem")(key, py_value);
  }

  std::string renameItem(const std::string& old_key, const std::string& new_name) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        std::string,
        ui::OutlinerModel,
        renameItem,
        old_key, new_name);
  }

  ui::outliner_factory_list_t getFactories(const std::string& parent_key) const override {
    py::gil_scoped_acquire acquire;
    // Call Python method and get list of dicts
    py::object py_result = py::cast(this).attr("getFactories")(parent_key);
    ui::outliner_factory_list_t factories;
    if (!py_result.is_none() && py::isinstance<py::list>(py_result)) {
      auto type_codec = python::pb11_typecodec_t::instance();
      for (auto item : py_result.cast<py::list>()) {
        if (py::isinstance<py::dict>(item)) {
          auto d = item.cast<py::dict>();
          ui::OutlinerFactory factory;
          if (d.contains("id")) factory.id = d["id"].cast<std::string>();
          if (d.contains("display_name")) factory.display_name = d["display_name"].cast<std::string>();
          if (d.contains("default_value") && !d["default_value"].is_none()) {
            factory.default_value = type_codec->decode(d["default_value"]);
          }
          factories.push_back(factory);
        }
      }
    }
    return factories;
  }

  std::string createItem(const std::string& parent_key, const std::string& name, const std::string& factory_id) override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        std::string,
        ui::OutlinerModel,
        createItem,
        parent_key, name, factory_id);
  }
};

void pyinit_ui_outliner(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // OutlinerModel base class (can be subclassed in Python)
  /////////////////////////////////////////////////////////////////////////////////
  auto outliner_model_type = //
      py::class_<ui::OutlinerModel, PyOutlinerModel, ui::outliner_model_ptr_t>(uimodule, "OutlinerModel")
          .def(py::init<>())
          .def("getChildren", &ui::OutlinerModel::getChildren)
          .def("getDisplayName", &ui::OutlinerModel::getDisplayName)
          .def("hasChildren", &ui::OutlinerModel::hasChildren)
          .def(
              "getValue",
              [type_codec](ui::outliner_model_ptr_t model, const std::string& key) -> py::object {
                auto value = model->getValue(key);
                if (!value.isSet()) {
                  return py::none();
                }
                return type_codec->encode(value);
              })
          .def(
              "addItem",
              [type_codec](ui::outliner_model_ptr_t model, const std::string& parent_key, const std::string& name, py::object py_value) {
                svar128_t value;
                if (!py_value.is_none()) {
                  value = type_codec->decode(py_value);
                }
                model->addItem(parent_key, name, value);
              },
              py::arg("parent_key"),
              py::arg("name"),
              py::arg("value") = py::none())
          .def("removeItem", &ui::OutlinerModel::removeItem)
          .def("moveItem", &ui::OutlinerModel::moveItem)
          .def(
              "updateItem",
              [type_codec](ui::outliner_model_ptr_t model, const std::string& key, py::object py_value) {
                svar128_t value;
                if (!py_value.is_none()) {
                  value = type_codec->decode(py_value);
                }
                model->updateItem(key, value);
              })
          .def("notifyItemAdded", &ui::OutlinerModel::notifyItemAdded)
          .def("notifyItemRemoved", &ui::OutlinerModel::notifyItemRemoved)
          .def("notifyItemChanged", &ui::OutlinerModel::notifyItemChanged)
          .def("notifyModelReset", &ui::OutlinerModel::notifyModelReset)
          .def_property(
              "allow_rename",
              &ui::OutlinerModel::allowRename,
              &ui::OutlinerModel::setAllowRename)
          .def_property(
              "allow_delete",
              &ui::OutlinerModel::allowDelete,
              &ui::OutlinerModel::setAllowDelete)
          .def_property(
              "allow_add",
              &ui::OutlinerModel::allowAdd,
              &ui::OutlinerModel::setAllowAdd)
          .def_property(
              "allow_multiselect",
              &ui::OutlinerModel::allowMultiSelect,
              &ui::OutlinerModel::setAllowMultiSelect)
          .def("renameItem", &ui::OutlinerModel::renameItem)
          .def(
              "getFactories",
              [](ui::outliner_model_ptr_t model, const std::string& parent_key) -> py::list {
                auto factories = model->getFactories(parent_key);
                py::list result;
                for (const auto& f : factories) {
                  py::dict d;
                  d["id"] = f.id;
                  d["display_name"] = f.display_name;
                  // Note: default_value conversion would need type_codec
                  result.append(d);
                }
                return result;
              })
          .def("createItem", &ui::OutlinerModel::createItem)
          .def("__repr__", [](ui::outliner_model_ptr_t model) {
            return FormatString("<OutlinerModel %p>", (void*)model.get());
          });

  type_codec->registerStdCodec<ui::outliner_model_ptr_t>(outliner_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // VarMapModel - built-in model backed by VarMap
  /////////////////////////////////////////////////////////////////////////////////
  auto varmap_model_type = //
      py::class_<ui::VarMapModel, ui::OutlinerModel, ui::varmap_model_ptr_t>(uimodule, "VarMapModel")
          .def(py::init<>())
          .def(py::init<varmap::varmap_ptr_t>())
          .def_property(
              "data",
              [](ui::varmap_model_ptr_t model) -> varmap::varmap_ptr_t {
                return model->getData();
              },
              [](ui::varmap_model_ptr_t model, varmap::varmap_ptr_t data) {
                model->setData(data);
              })
          .def("__repr__", [](ui::varmap_model_ptr_t model) {
            return FormatString("<VarMapModel %p>", (void*)model.get());
          });

  type_codec->registerStdCodec<ui::varmap_model_ptr_t>(varmap_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // Outliner widget
  /////////////////////////////////////////////////////////////////////////////////
  auto outliner_type = //
      py::class_<ui::Outliner, ui::Widget, ui::outliner_ptr_t>(uimodule, "Outliner")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::outliner_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto outliner     = std::make_shared<ui::Outliner>(name);
                return outliner;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t { //
                auto decoded_args = type_codec->decodeList(py_args);
                auto name         = decoded_args[0].get<std::string>();
                auto layoutitem   = lg->makeChild<ui::Outliner>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "model",
              [](ui::outliner_ptr_t outliner) -> ui::outliner_model_ptr_t { //
                return outliner->getModel();
              },
              [](ui::outliner_ptr_t outliner, ui::outliner_model_ptr_t model) { //
                outliner->setModel(model);
              })
          .def_property(
              "data",
              [](ui::outliner_ptr_t outliner) -> varmap::varmap_ptr_t { //
                return outliner->getData();
              },
              [](ui::outliner_ptr_t outliner, varmap::varmap_ptr_t data) { //
                outliner->setData(data);
              })
          .def_property(
              "selected_key",
              [](ui::outliner_ptr_t outliner) -> std::string { //
                return outliner->getSelectedKey();
              },
              [](ui::outliner_ptr_t outliner, const std::string& key) { //
                outliner->setSelectedKey(key);
              })
          .def_property(
              "selected_keys",
              [](ui::outliner_ptr_t outliner) -> py::list { //
                py::list result;
                for (const auto& key : outliner->getSelectedKeys()) {
                  result.append(key);
                }
                return result;
              },
              [](ui::outliner_ptr_t outliner, py::list keys) { //
                outliner->clearSelection();
                for (auto key : keys) {
                  outliner->addToSelection(key.cast<std::string>());
                }
              })
          .def("addToSelection", &ui::Outliner::addToSelection)
          .def("removeFromSelection", &ui::Outliner::removeFromSelection)
          .def("clearSelection", &ui::Outliner::clearSelection)
          .def("isSelected", &ui::Outliner::isSelected)
          .def(
              "setExpanded",
              [](ui::outliner_ptr_t outliner, const std::string& key, bool expanded) { //
                outliner->setExpanded(key, expanded);
              })
          .def(
              "isExpanded",
              [](ui::outliner_ptr_t outliner, const std::string& key) -> bool { //
                return outliner->isExpanded(key);
              })
          .def(
              "expandAll",
              [](ui::outliner_ptr_t outliner) { //
                outliner->expandAll();
              })
          .def(
              "collapseAll",
              [](ui::outliner_ptr_t outliner) { //
                outliner->collapseAll();
              })
          .def(
              "onSelect",
              [](ui::outliner_ptr_t outliner, py::object callback) { //
                outliner->_onSelect = [callback](const std::string& key) {
                  py::gil_scoped_acquire acquire;
                  callback(key);
                };
              })
          .def(
              "onRename",
              [](ui::outliner_ptr_t outliner, py::object callback) { //
                outliner->_onRename = [callback](const std::string& old_key, const std::string& new_name) {
                  py::gil_scoped_acquire acquire;
                  callback(old_key, new_name);
                };
              })
          .def(
              "onDelete",
              [](ui::outliner_ptr_t outliner, py::object callback) { //
                outliner->_onDelete = [callback](const std::string& key) {
                  py::gil_scoped_acquire acquire;
                  callback(key);
                };
              })
          .def(
              "onAdd",
              [](ui::outliner_ptr_t outliner, py::object callback) { //
                outliner->_onAdd = [callback](const std::string& key) {
                  py::gil_scoped_acquire acquire;
                  callback(key);
                };
              })
          .def("startEditing", &ui::Outliner::startEditing)
          .def("cancelEditing", &ui::Outliner::cancelEditing)
          .def("commitEditing", &ui::Outliner::commitEditing)
          .def("isEditing", &ui::Outliner::isEditing)
          .def("startAdding", &ui::Outliner::startAdding)
          .def("cancelAdding", &ui::Outliner::cancelAdding)
          .def("commitAdding", &ui::Outliner::commitAdding)
          .def("isAdding", &ui::Outliner::isAdding)
          .def_property(
              "item_height",
              [](ui::outliner_ptr_t outliner) -> int { //
                return outliner->_item_height;
              },
              [](ui::outliner_ptr_t outliner, int h) { //
                outliner->_item_height = h;
              })
          .def_property(
              "indent_width",
              [](ui::outliner_ptr_t outliner) -> int { //
                return outliner->_indent_width;
              },
              [](ui::outliner_ptr_t outliner, int w) { //
                outliner->_indent_width = w;
              })
          .def_property(
              "bgcolor",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_bgcolor;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_bgcolor = c;
              })
          .def_property(
              "text_color",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_text_color;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_text_color = c;
              })
          .def_property(
              "selected_color",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_selected_color;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_selected_color = c;
              })
          .def_property(
              "hover_color",
              [](ui::outliner_ptr_t outliner) -> fvec4 { //
                return outliner->_hover_color;
              },
              [](ui::outliner_ptr_t outliner, fvec4 c) { //
                outliner->_hover_color = c;
              })
          .def_property(
              "font",
              [](ui::outliner_ptr_t outliner) -> font_ptr_t { //
                return outliner->_font;
              },
              [](ui::outliner_ptr_t outliner, font_ptr_t font) { //
                outliner->_font = font;
              })
          .def_readwrite("draw_background", &ui::Outliner::_draw_background)
          .def("__repr__", [](ui::outliner_ptr_t outliner) {
            return FormatString("<Outliner name<%s> widget<%p>>", outliner->GetName().c_str(), (void*)outliner.get());
          });

  type_codec->registerStdCodec<ui::outliner_ptr_t>(outliner_type);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
