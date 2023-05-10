///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/util/hotkey.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork {
//using namespace rtti;
///////////////////////////////////////////////////////////////////////////////
using class_pyptr_t               = ork::python::unmanaged_ptr<rtti::Class>;
///////////////////////////////////////////////////////////////////////////////
void pyinit_reflection(py::module& module_core) {
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
    auto class_type_t = py::class_<class_pyptr_t>(module_core, "Class") //
      .def_property_readonly("name", [](class_pyptr_t clazz) -> std::string {
        return clazz->Name().c_str();
      });
  type_codec->registerStdCodec<class_pyptr_t>(class_type_t);
  /////////////////////////////////////////////////////////////////////////////////
    auto icastable_type_t = py::class_<rtti::ICastable,rtti::castable_ptr_t>(module_core, "ICastable") //
      .def_property_readonly("clazz", [](rtti::castable_ptr_t castable) -> class_pyptr_t {
        return castable->GetClass(); 
      });
  type_codec->registerStdCodec<rtti::castable_ptr_t>(icastable_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto objtype_t = py::class_<Object,rtti::ICastable,object_ptr_t>(module_core, "Object"); //
  type_codec->registerStdCodec<object_ptr_t>(objtype_t);
  /////////////////////////////////////////////////////////////////////////////////
  using hotkey_ptr_t = std::shared_ptr<HotKey>;
  auto hkey_type =                                                              //
      py::class_<HotKey,Object,hotkey_ptr_t>(module_core, "HotKey") //
          .def(py::init<>());
  type_codec->registerStdCodec<hotkey_ptr_t>(hkey_type);
  /////////////////////////////////////////////////////////////////////////////////
  using hotkeyconfig_ptr_t = std::shared_ptr<HotKeyConfiguration>;
  auto hkeycfg_type =                                                              //
      py::class_<HotKeyConfiguration,Object,hotkeyconfig_ptr_t>(module_core, "HotKeyConfiguration") //
          .def(py::init<>());
  type_codec->registerStdCodec<hotkeyconfig_ptr_t>(hkeycfg_type);

}

///////////////////////////////////////////////////////////////////////////////
} 
