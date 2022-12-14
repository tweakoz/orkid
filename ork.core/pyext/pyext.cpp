////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
struct CorePythonApplication  {

  CorePythonApplication(){
        _stringpoolctx = std::make_shared<ork::StringPoolContext>();
    StringPoolStack::Push(_stringpoolctx);
  }
  ~CorePythonApplication(){
    StringPoolStack::Pop();
  }
  stringpoolctx_ptr_t _stringpoolctx;
};
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math(py::module& module_core);

static void _coreappinit() {
  SetCurrentThreadName("main");

  static CorePythonApplication the_app;

  static auto WorkingDirContext = std::make_shared<FileDevContext>();
  OldSchool::SetGlobalPathVariable("data://", file::Path::orkroot_dir());

  rtti::Class::InitializeClasses();
}

static PoolString _addpoolstring(std::string str) {
  return AddPooledString(str.c_str());
}

///////////////////////////////////////////////////////////////////////////////

PYBIND11_MODULE(_core, module_core) {
  module_core.doc() = "Orkid Core Library (math,kernel,reflection,ect..)";
  /////////////////////////////////////////////////////////////////////////////////
  module_core.def("coreappinit", &_coreappinit);
  module_core.def("addpoolstring", &_addpoolstring);
  /////////////////////////////////////////////////////////////////////////////////
  // core decoder tyoes
  /////////////////////////////////////////////////////////////////////////////////
  auto type_codec = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////////////////////////
  using coreapp_ptr_t = std::shared_ptr<CorePythonApplication>;
  auto application_type = py::class_<CorePythonApplication,coreapp_ptr_t>(module_core, "Application")
      .def(
          "__repr__",
          [](coreapp_ptr_t app) -> std::string {
            fxstring<256> fxs;
            fxs.format("OrkPyCoreApp(%p)", (void*) app.get());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<coreapp_ptr_t>(application_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto crcstr_type =                                                   //
      py::class_<CrcString, crcstring_ptr_t>(module_core, "CrcString") //
          .def(py::init<>([](std::string str)->crcstring_ptr_t{
            return std::make_shared<CrcString>(str.c_str());
          }))
          .def_property_readonly(
              "hashed",
              [](crcstring_ptr_t s) -> uint64_t { //
                return s->hashed();
              })
          .def("__repr__", [](crcstring_ptr_t s) -> std::string {
            fxstring<64> fxs;
            fxs.format("CrcString(0x%zx:%zu)", s->hashed(), s->hashed());
            return fxs.c_str();
          });
  type_codec->registerStdCodec<crcstring_ptr_t>(crcstr_type);
  /////////////////////////////////////////////////////////////////////////////////
  struct CrcStringProxy {};
  using crcstrproxy_ptr_t = std::shared_ptr<CrcStringProxy>;
  auto crcstrproxy_type   =                                                        //
      py::class_<CrcStringProxy, crcstrproxy_ptr_t>(module_core, "CrcStringProxy") //
          .def(py::init<>())
          .def(
              "__getattr__",                                                           //
              [](crcstrproxy_ptr_t proxy, const std::string& key) -> crcstring_ptr_t { //
                return std::make_shared<CrcString>(key.c_str());
              });
  type_codec->registerStdCodec<crcstrproxy_ptr_t>(crcstrproxy_type);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<PoolString>(module_core, "PoolString") //
      .def("__repr__", [](const PoolString& s) -> std::string {
        fxstring<64> fxs;
        fxs.format("PoolString(%s)", s.c_str());
        return fxs.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<file::Path>(module_core, "Path")
      .def(py::init<std::string>())
      .def("isAbsolute", &file::Path::isAbsolute)
      .def("IsRelative", &file::Path::isRelative)
      .def("__repr__", [](const file::Path& s) -> std::string {
        fxstring<64> fxs;
        fxs.format("Path(%s)", s.c_str());
        return fxs.c_str();
      });

  /////////////////////////////////////////////////////////////////////////////////
  py::class_<Object>(module_core, "Object") //
      .def("clazz", [](Object* o) -> std::string {
        auto clazz = rtti::downcast<object::ObjectClass*>(o->GetClass());
        auto name  = clazz->Name();
        return name.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  auto varmaptype_t =                                                         //
      py::class_<varmap::VarMap, varmap::varmap_ptr_t>(module_core, "VarMap") //
          .def(py::init<>())
          .def(
              "__setattr__",                                                                    //
              [type_codec](varmap::varmap_ptr_t vmap, const std::string& key, py::object val) { //
                auto varmap_val = type_codec->decode(val);
                vmap->setValueForKey(key, varmap_val);
              })
          .def(
              "__getattr__",                                                                  //
              [type_codec](varmap::varmap_ptr_t vmap, const std::string& key) -> py::object { //
                auto varmap_val = vmap->valueForKey(key);
                auto python_val = type_codec->encode(varmap_val);
                return python_val;
              });
  type_codec->registerStdCodec<varmap::varmap_ptr_t>(varmaptype_t);
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_math(module_core);
  /////////////////////////////////////////////////////////////////////////////////
}; // PYBIND11_MODULE(_core, module_core) {

} // namespace ork
