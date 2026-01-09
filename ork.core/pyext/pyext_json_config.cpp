////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/json_config.h>

namespace ork {

void pyinit_json_config(py::module& module_core) {

  /////////////////////////////////////////////////////////////////////////////////
  // JsonConfig
  /////////////////////////////////////////////////////////////////////////////////

  auto json_config_type = py::class_<JsonConfig, json_config_ptr_t>(module_core, "JsonConfig")
      .def(py::init<>())

      // Factory methods
      .def_static("instance", &JsonConfig::instance,
          py::arg("name"),
          "Get or create a named config (cached by path)")
      .def_static("defaultConfig", &JsonConfig::defaultConfig,
          "Get the default application config")
      .def_static("setConfigDirectory", &JsonConfig::setConfigDirectory,
          py::arg("dir"),
          "Set the config directory")
      .def_static("getConfigDirectory", &JsonConfig::getConfigDirectory,
          "Get the config directory")

      // Load/Save
      .def("load", &JsonConfig::load,
          py::arg("filepath"),
          "Load config from file (creates if missing)")
      .def("save", &JsonConfig::save,
          "Save config to file (atomic write)")
      .def("saveNow", &JsonConfig::saveNow,
          "Force immediate save")
      .def_property_readonly("filepath", &JsonConfig::getFilePath,
          "Get the file path")

      // String values
      .def("setString", &JsonConfig::setString,
          py::arg("key"), py::arg("value"))
      .def("getString", &JsonConfig::getString,
          py::arg("key"), py::arg("default_value") = "")
      .def("hasKey", &JsonConfig::hasKey,
          py::arg("key"))
      .def("removeKey", &JsonConfig::removeKey,
          py::arg("key"))

      // Numeric values
      .def("setInt", &JsonConfig::setInt,
          py::arg("key"), py::arg("value"))
      .def("getInt", &JsonConfig::getInt,
          py::arg("key"), py::arg("default_value") = 0)
      .def("setFloat", &JsonConfig::setFloat,
          py::arg("key"), py::arg("value"))
      .def("getFloat", &JsonConfig::getFloat,
          py::arg("key"), py::arg("default_value") = 0.0)
      .def("setBool", &JsonConfig::setBool,
          py::arg("key"), py::arg("value"))
      .def("getBool", &JsonConfig::getBool,
          py::arg("key"), py::arg("default_value") = false)

      // String list values
      .def("setStringList", &JsonConfig::setStringList,
          py::arg("key"), py::arg("values"))
      .def("getStringList", &JsonConfig::getStringList,
          py::arg("key"))
      .def("appendToStringList", &JsonConfig::appendToStringList,
          py::arg("key"), py::arg("value"))
      .def("removeFromStringList", &JsonConfig::removeFromStringList,
          py::arg("key"), py::arg("value"))
      .def("stringListContains", &JsonConfig::stringListContains,
          py::arg("key"), py::arg("value"))

      // Nested objects
      .def("getObject", &JsonConfig::getObject,
          py::arg("key"),
          "Get a nested config object (creates if missing)")
      .def("setObject", &JsonConfig::setObject,
          py::arg("key"), py::arg("obj"),
          "Set a nested config object")

      // Python dict-like access
      .def("__getitem__", [](json_config_ptr_t cfg, const std::string& key) -> py::object {
        if (!cfg->hasKey(key)) {
          throw py::key_error(key);
        }
        // Try string first
        auto str_val = cfg->getString(key, "\x00__NOT_FOUND__\x00");
        if (str_val != "\x00__NOT_FOUND__\x00") {
          return py::cast(str_val);
        }
        // Try list
        auto list_val = cfg->getStringList(key);
        if (!list_val.empty()) {
          return py::cast(list_val);
        }
        // Try int
        auto int_val = cfg->getInt(key, INT64_MIN);
        if (int_val != INT64_MIN) {
          return py::cast(int_val);
        }
        // Try float
        auto float_val = cfg->getFloat(key, std::numeric_limits<double>::quiet_NaN());
        if (!std::isnan(float_val)) {
          return py::cast(float_val);
        }
        return py::none();
      })
      .def("__setitem__", [](json_config_ptr_t cfg, const std::string& key, py::object value) {
        if (py::isinstance<py::str>(value)) {
          cfg->setString(key, py::cast<std::string>(value));
        } else if (py::isinstance<py::int_>(value)) {
          cfg->setInt(key, py::cast<int64_t>(value));
        } else if (py::isinstance<py::float_>(value)) {
          cfg->setFloat(key, py::cast<double>(value));
        } else if (py::isinstance<py::bool_>(value)) {
          cfg->setBool(key, py::cast<bool>(value));
        } else if (py::isinstance<py::list>(value)) {
          std::vector<std::string> list;
          for (auto item : value) {
            list.push_back(py::cast<std::string>(item));
          }
          cfg->setStringList(key, list);
        }
      })
      .def("__contains__", &JsonConfig::hasKey)
      .def("__delitem__", &JsonConfig::removeKey)

      .def("__repr__", [](json_config_ptr_t cfg) -> std::string {
        return FormatString("JsonConfig(%s)", cfg->getFilePath().c_str());
      });

  /////////////////////////////////////////////////////////////////////////////////
}

} // namespace ork
