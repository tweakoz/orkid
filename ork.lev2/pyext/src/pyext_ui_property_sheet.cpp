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
#include <ork/lev2/ui/property_sheet.h>
#include <ork/lev2/ui/reflection_property_model.h>
#include <ork/python/gil_safe_pyobj.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {

// Trampoline class for Python subclassing of PropertySheetModel
class PyPropertySheetModel : public ui::PropertySheetModel {
public:
  using ui::PropertySheetModel::PropertySheetModel;

  std::vector<std::string> getChildren(const std::string& parent_key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::vector<std::string>,
        ui::PropertySheetModel,
        getChildren,
        parent_key);
  }

  std::string getDisplayName(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        std::string,
        ui::PropertySheetModel,
        getDisplayName,
        key);
  }

  bool hasChildren(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        bool,
        ui::PropertySheetModel,
        hasChildren,
        key);
  }

  svar128_t getValue(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    py::object py_result = py::cast(this).attr("getValue")(key);
    if (py_result.is_none()) {
      return svar128_t();
    }
    auto type_codec = python::pb11_typecodec_t::instance();
    return type_codec->decode(py_result);
  }

  void setValue(const std::string& key, svar128_t value) override {
    py::gil_scoped_acquire acquire;
    auto type_codec = python::pb11_typecodec_t::instance();
    py::object py_value = type_codec->encode(value);
    py::cast(this).attr("setValue")(key, py_value);
  }

  ui::PropertyType getPropertyType(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE_PURE(
        ui::PropertyType,
        ui::PropertySheetModel,
        getPropertyType,
        key);
  }

  varmap::varmap_ptr_t getAnnotations(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    py::object py_result = py::cast(this).attr("getAnnotations")(key);
    if (py_result.is_none()) {
      return nullptr;
    }
    // Try to cast to varmap_ptr_t
    try {
      return py_result.cast<varmap::varmap_ptr_t>();
    } catch (...) {
      return nullptr;
    }
  }

  std::vector<std::string> getChoices(const std::string& key) const override {
    py::gil_scoped_acquire acquire;
    PYBIND11_OVERRIDE(
        std::vector<std::string>,
        ui::PropertySheetModel,
        getChoices,
        key);
  }
};

void pyinit_ui_property_sheet(py::module& uimodule) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // PropertyType enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<ui::PropertyType>(uimodule, "PropertyType")
      .value("Unknown", ui::PropertyType::Unknown)
      .value("Bool", ui::PropertyType::Bool)
      .value("Int", ui::PropertyType::Int)
      .value("Float", ui::PropertyType::Float)
      .value("String", ui::PropertyType::String)
      .value("Vec2", ui::PropertyType::Vec2)
      .value("Vec3", ui::PropertyType::Vec3)
      .value("Vec4", ui::PropertyType::Vec4)
      .value("Color", ui::PropertyType::Color)
      .value("Asset", ui::PropertyType::Asset)
      .value("Quat", ui::PropertyType::Quat)
      .value("Enum", ui::PropertyType::Enum)
      .value("Group", ui::PropertyType::Group);

  /////////////////////////////////////////////////////////////////////////////////
  // PropertySheetModel base class (can be subclassed in Python)
  /////////////////////////////////////////////////////////////////////////////////
  auto property_model_type = //
      py::class_<ui::PropertySheetModel, PyPropertySheetModel, ui::property_sheet_model_ptr_t>(uimodule, "PropertySheetModel")
          .def(py::init<>())
          .def("getChildren", &ui::PropertySheetModel::getChildren)
          .def("getDisplayName", &ui::PropertySheetModel::getDisplayName)
          .def("hasChildren", &ui::PropertySheetModel::hasChildren)
          .def(
              "getValue",
              [type_codec](ui::property_sheet_model_ptr_t model, const std::string& key) -> py::object {
                auto value = model->getValue(key);
                if (!value.isSet()) {
                  return py::none();
                }
                return type_codec->encode(value);
              })
          .def(
              "setValue",
              [type_codec](ui::property_sheet_model_ptr_t model, const std::string& key, py::object py_value) {
                svar128_t value;
                if (!py_value.is_none()) {
                  value = type_codec->decode(py_value);
                }
                model->setValue(key, value);
              })
          .def("getPropertyType", &ui::PropertySheetModel::getPropertyType)
          .def(
              "getAnnotations",
              [](ui::property_sheet_model_ptr_t model, const std::string& key) -> varmap::varmap_ptr_t {
                return model->getAnnotations(key);
              })
          .def("getChoices", &ui::PropertySheetModel::getChoices)
          .def("notifyPropertyChanged", &ui::PropertySheetModel::notifyPropertyChanged)
          .def("notifyStructureChanged", &ui::PropertySheetModel::notifyStructureChanged)
          .def("notifyExternalValueChanged", &ui::PropertySheetModel::notifyExternalValueChanged)
          .def_property(
              "read_only",
              &ui::PropertySheetModel::isReadOnly,
              &ui::PropertySheetModel::setReadOnly)
          .def("__repr__", [](ui::property_sheet_model_ptr_t model) {
            return FormatString("<PropertySheetModel %p>", (void*)model.get());
          });

  type_codec->registerStdCodec<ui::property_sheet_model_ptr_t>(property_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // VarMapPropertyModel - built-in model backed by VarMap
  /////////////////////////////////////////////////////////////////////////////////
  auto varmap_property_model_type = //
      py::class_<ui::VarMapPropertyModel, ui::PropertySheetModel, ui::varmap_property_model_ptr_t>(uimodule, "VarMapPropertyModel")
          .def(py::init<>())
          .def(py::init<varmap::varmap_ptr_t>())
          .def_property(
              "data",
              [](ui::varmap_property_model_ptr_t model) -> varmap::varmap_ptr_t {
                return model->getData();
              },
              [](ui::varmap_property_model_ptr_t model, varmap::varmap_ptr_t data) {
                model->setData(data);
              })
          .def("setAnnotations", &ui::VarMapPropertyModel::setAnnotations)
          .def("__repr__", [](ui::varmap_property_model_ptr_t model) {
            return FormatString("<VarMapPropertyModel %p>", (void*)model.get());
          });

  type_codec->registerStdCodec<ui::varmap_property_model_ptr_t>(varmap_property_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ReflectionPropertySheetModel - reflection-driven model
  /////////////////////////////////////////////////////////////////////////////////
  auto refl_model_type = //
      py::class_<ui::ReflectionPropertySheetModel,
                 ui::PropertySheetModel,
                 ui::reflection_property_model_ptr_t>(uimodule, "ReflectionPropertySheetModel")
          .def(py::init<>())
          .def_property(
              "object",
              &ui::ReflectionPropertySheetModel::getObject,
              &ui::ReflectionPropertySheetModel::setObject)
          .def("isMapProperty", &ui::ReflectionPropertySheetModel::isMapProperty)
          .def("isMapPropertyConst", &ui::ReflectionPropertySheetModel::isMapConst)
          .def("addMapElement", &ui::ReflectionPropertySheetModel::addMapElement)
          .def("removeMapElement", &ui::ReflectionPropertySheetModel::removeMapElement)
          .def("renameMapElement", &ui::ReflectionPropertySheetModel::renameMapElement)
          .def(
              "addKeyOverride",
              [type_codec](
                  ui::reflection_property_model_ptr_t model,
                  const std::string& key,
                  ui::PropertyType type,
                  py::object py_getter,
                  py::object py_setter,
                  py::object py_choices) {
                ui::ReflectionPropertySheetModel::KeyOverride ovr;
                ovr.type = type;
                if (!py_getter.is_none()) {
                  ovr.getter = [py_getter, type_codec]() -> svar128_t {
                    py::gil_scoped_acquire acquire;
                    py::object result = py_getter();
                    if (result.is_none()) return svar128_t();
                    return type_codec->decode(result);
                  };
                }
                if (!py_setter.is_none()) {
                  ovr.setter = [py_setter, type_codec](svar128_t value) {
                    py::gil_scoped_acquire acquire;
                    py::object py_value = type_codec->encode(value);
                    py_setter(py_value);
                  };
                }
                if (!py_choices.is_none()) {
                  ovr.choices = [py_choices]() -> std::vector<std::string> {
                    py::gil_scoped_acquire acquire;
                    py::object result = py_choices();
                    return result.cast<std::vector<std::string>>();
                  };
                }
                model->addKeyOverride(key, ovr);
              },
              py::arg("key"),
              py::arg("type"),
              py::arg("getter"),
              py::arg("setter"),
              py::arg("choices"))
          .def("clearKeyOverrides", &ui::ReflectionPropertySheetModel::clearKeyOverrides)
          .def("__repr__", [](ui::reflection_property_model_ptr_t model) {
            return FormatString("<ReflectionPropertySheetModel %p>", (void*)model.get());
          });

  type_codec->registerStdCodec<ui::reflection_property_model_ptr_t>(refl_model_type);

  /////////////////////////////////////////////////////////////////////////////////
  // PropertySheet widget
  /////////////////////////////////////////////////////////////////////////////////
  auto property_sheet_type = //
      py::class_<ui::PropertySheet, ui::Group, ui::property_sheet_ptr_t>(uimodule, "PropertySheet")
          .def_static(
              "wfactory",
              [type_codec](py::list py_args) -> ui::property_sheet_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto sheet = std::make_shared<ui::PropertySheet>(name);
                return sheet;
              })
          .def_static(
              "uifactory",
              [type_codec](uilayoutgroup_ptr_t lg, py::list py_args) -> uilayoutitem_ptr_t {
                auto decoded_args = type_codec->decodeList(py_args);
                auto name = decoded_args[0].get<std::string>();
                auto layoutitem = lg->makeChild<ui::PropertySheet>(name);
                return layoutitem.as_shared();
              })
          .def_property(
              "model",
              [](ui::property_sheet_ptr_t sheet) -> ui::property_sheet_model_ptr_t {
                return sheet->getModel();
              },
              [](ui::property_sheet_ptr_t sheet, ui::property_sheet_model_ptr_t model) {
                sheet->setModel(model);
              })
          .def_property(
              "data",
              [](ui::property_sheet_ptr_t sheet) -> varmap::varmap_ptr_t {
                return sheet->getData();
              },
              [](ui::property_sheet_ptr_t sheet, varmap::varmap_ptr_t data) {
                sheet->setData(data);
              })
          .def("setExpanded", &ui::PropertySheet::setExpanded)
          .def("isExpanded", &ui::PropertySheet::isExpanded)
          .def("expandAll", &ui::PropertySheet::expandAll)
          .def("collapseAll", &ui::PropertySheet::collapseAll)
          .def("rebuild", &ui::PropertySheet::rebuild)
          .def(
              "onPropertyChanged",
              [](ui::property_sheet_ptr_t sheet, py::object callback) {
                auto safe = python::gil_safe_pyobj(callback);
                sheet->_onPropertyChanged = [safe](const std::string& key, svar128_t value) {
                  py::gil_scoped_acquire acquire;
                  auto fn = safe.valueAs<py::object>();
                  auto type_codec = python::pb11_typecodec_t::instance();
                  py::object py_value = type_codec->encode(value);
                  (*fn)(key, py_value);
                };
              })
          .def_property(
              "row_height",
              [](ui::property_sheet_ptr_t sheet) -> int { return sheet->_row_height; },
              [](ui::property_sheet_ptr_t sheet, int h) { sheet->_row_height = h; })
          .def_property(
              "label_width",
              [](ui::property_sheet_ptr_t sheet) -> int { return sheet->_label_width; },
              [](ui::property_sheet_ptr_t sheet, int w) { sheet->_label_width = w; })
          .def_property(
              "indent_width",
              [](ui::property_sheet_ptr_t sheet) -> int { return sheet->_indent_width; },
              [](ui::property_sheet_ptr_t sheet, int w) { sheet->_indent_width = w; })
          .def_property(
              "bgcolor",
              [](ui::property_sheet_ptr_t sheet) -> fvec4 { return sheet->_bgcolor; },
              [](ui::property_sheet_ptr_t sheet, fvec4 c) { sheet->_bgcolor = c; })
          .def_property(
              "label_color",
              [](ui::property_sheet_ptr_t sheet) -> fvec4 { return sheet->_label_color; },
              [](ui::property_sheet_ptr_t sheet, fvec4 c) { sheet->_label_color = c; })
          .def_property(
              "group_color",
              [](ui::property_sheet_ptr_t sheet) -> fvec4 { return sheet->_group_color; },
              [](ui::property_sheet_ptr_t sheet, fvec4 c) { sheet->_group_color = c; })
          // Detail editor overlay
          .def(
              "showDetailEditor",
              [](ui::property_sheet_ptr_t sheet, const std::string& key, ui::widget_ptr_t editor) {
                sheet->showDetailEditor(key, editor);
              })
          .def("closeDetailEditor", &ui::PropertySheet::closeDetailEditor)
          .def("isDetailEditorActive", &ui::PropertySheet::isDetailEditorActive)
          .def_property(
              "detail_height_ratio",
              [](ui::property_sheet_ptr_t sheet) -> float { return sheet->_detail_height_ratio; },
              [](ui::property_sheet_ptr_t sheet, float r) { sheet->_detail_height_ratio = r; })
          .def_property(
              "detail_min_height",
              [](ui::property_sheet_ptr_t sheet) -> int { return sheet->_detail_min_height; },
              [](ui::property_sheet_ptr_t sheet, int h) { sheet->_detail_min_height = h; })
          .def_property(
              "detail_bg_color",
              [](ui::property_sheet_ptr_t sheet) -> fvec4 { return sheet->_detail_bg_color; },
              [](ui::property_sheet_ptr_t sheet, fvec4 c) { sheet->_detail_bg_color = c; })
          .def(
              "onRequestCustomEditor",
              [](ui::property_sheet_ptr_t sheet, py::object callback) {
                if (not callback.is_none()) {
                  auto safe = python::gil_safe_pyobj(callback);
                  sheet->_onRequestCustomEditor = [safe](const std::string& key, const std::string& editor_id) {
                    py::gil_scoped_acquire acquire;
                    auto fn = safe.valueAs<py::object>();
                    (*fn)(key, editor_id);
                  };
                }
              })
          .def(
              "onRequestDetailEditor",
              [type_codec](ui::property_sheet_ptr_t sheet, py::object callback) {
                if (not callback.is_none()) {
                  auto safe = python::gil_safe_pyobj(callback);
                  sheet->_onRequestDetailEditor = [safe, type_codec](
                                                       const std::string& key,
                                                       ui::PropertyType type,
                                                       svar128_t value) {
                    py::gil_scoped_acquire acquire;
                    auto fn = safe.valueAs<py::object>();
                    py::object py_value = type_codec->encode(value);
                    (*fn)(key, type, py_value);
                  };
                }
              })
          .def(
              "getDetailBinding",
              [type_codec](ui::property_sheet_ptr_t sheet) -> py::object {
                auto binding = sheet->getDetailBinding();
                if (!binding) {
                  return py::none();
                }
                // Return a dict with binding info
                py::dict result;
                result["property_key"] = binding->property_key;
                result["property_type"] = binding->property_type;
                result["initial_value"] = type_codec->encode(binding->initial_value);
                // Wrap callbacks
                result["onValueChanged"] = py::cpp_function([binding, type_codec](py::object py_value) {
                  if (binding->onValueChanged) {
                    svar128_t value = type_codec->decode(py_value);
                    binding->onValueChanged(value);
                  }
                });
                result["onValueCommit"] = py::cpp_function([binding, type_codec](py::object py_value) {
                  if (binding->onValueCommit) {
                    svar128_t value = type_codec->decode(py_value);
                    binding->onValueCommit(value);
                  }
                });
                result["onCancel"] = py::cpp_function([binding]() {
                  if (binding->onCancel) {
                    binding->onCancel();
                  }
                });
                result["onClose"] = py::cpp_function([binding]() {
                  if (binding->onClose) {
                    binding->onClose();
                  }
                });
                return result;
              })
          // Factory registration
          .def(
              "registerEditorFactory",
              [type_codec](
                  ui::property_sheet_ptr_t sheet,
                  py::object type_or_crc,
                  py::object inline_factory,
                  py::object detail_factory) {
                // Get the CRC value from CrcString
                auto crcstr = py::cast<crcstring_ptr_t>(type_or_crc);
                uint32_t type_crc = crcstr->hashed();

                // Create inline factory wrapper
                ui::inline_editor_factory_t cpp_inline_factory = nullptr;
                if (!inline_factory.is_none()) {
                  cpp_inline_factory = [inline_factory, type_codec](
                                           ui::property_sheet_ptr_t sheet,
                                           const std::string& key,
                                           svar128_t value,
                                           varmap::varmap_ptr_t annotations) -> ui::widget_ptr_t {
                    py::gil_scoped_acquire acquire;
                    py::object py_value = type_codec->encode(value);
                    py::object result = inline_factory(sheet, key, py_value, annotations);
                    if (result.is_none()) {
                      return nullptr;
                    }
                    return result.cast<ui::widget_ptr_t>();
                  };
                }

                // Create detail factory wrapper
                ui::detail_editor_factory_t cpp_detail_factory = nullptr;
                if (!detail_factory.is_none()) {
                  cpp_detail_factory = [detail_factory, type_codec](
                                           ui::property_sheet_ptr_t sheet,
                                           const std::string& key,
                                           svar128_t value,
                                           varmap::varmap_ptr_t annotations,
                                           ui::detail_editor_binding_ptr_t binding) -> ui::widget_ptr_t {
                    py::gil_scoped_acquire acquire;
                    py::object py_value = type_codec->encode(value);
                    // Create binding dict for Python
                    py::dict py_binding;
                    py_binding["property_key"] = binding->property_key;
                    py_binding["property_type"] = binding->property_type;
                    py_binding["initial_value"] = type_codec->encode(binding->initial_value);
                    py_binding["onValueChanged"] = py::cpp_function([binding, type_codec](py::object py_val) {
                      if (binding->onValueChanged) {
                        binding->onValueChanged(type_codec->decode(py_val));
                      }
                    });
                    py_binding["onValueCommit"] = py::cpp_function([binding, type_codec](py::object py_val) {
                      if (binding->onValueCommit) {
                        binding->onValueCommit(type_codec->decode(py_val));
                      }
                    });
                    py_binding["onCancel"] = py::cpp_function([binding]() {
                      if (binding->onCancel) {
                        binding->onCancel();
                      }
                    });
                    py_binding["onClose"] = py::cpp_function([binding]() {
                      if (binding->onClose) {
                        binding->onClose();
                      }
                    });

                    py::object result = detail_factory(sheet, key, py_value, annotations, py_binding);
                    if (result.is_none()) {
                      return nullptr;
                    }
                    return result.cast<ui::widget_ptr_t>();
                  };
                }

                sheet->registerEditorFactory(type_crc, cpp_inline_factory, cpp_detail_factory);
              },
              py::arg("type_or_crc"),
              py::arg("inline_factory"),
              py::arg("detail_factory") = py::none())
          .def(
              "hasEditorFactory",
              [](ui::property_sheet_ptr_t sheet, crcstring_ptr_t type_crc) -> bool {
                return sheet->hasEditorFactory(type_crc->hashed());
              })
          .def("requestDetailEditor", &ui::PropertySheet::requestDetailEditor)
          .def("__repr__", [](ui::property_sheet_ptr_t sheet) {
            return FormatString("<PropertySheet name<%s> widget<%p>>", sheet->GetName().c_str(), (void*)sheet.get());
          });

  type_codec->registerStdCodec<ui::property_sheet_ptr_t>(property_sheet_type);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
