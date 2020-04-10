#include "pyext.h"
///////////////////////////////////////////////////////////////////////////////
class CorePythonApplication : public ork::Application {
  RttiDeclareConcrete(CorePythonApplication, ork::Application);
};

void CorePythonApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(CorePythonApplication, "CorePythonApplication");
///////////////////////////////////////////////////////////////////////////////
namespace ork {
void pyinit_math(py::module& module_core);

static void _coreappinit() {
  SetCurrentThreadName("main");

  static CorePythonApplication the_app;
  ApplicationStack::Push(&the_app);

  static auto WorkingDirContext = std::make_shared<FileDevContext>();
  auto base_dir                 = file::GetStartupDirectory();
  if (getenv("ORKID_WORKSPACE_DIR") != nullptr)
    base_dir = getenv("ORKID_WORKSPACE_DIR");
  OldSchool::SetGlobalStringVariable("data://", base_dir.c_str());
  printf("ORKID_WORKSPACE_DIR<%s>\n", base_dir.c_str());

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
      .def("isAbsolute", &file::Path::IsAbsolute)
      .def("IsRelative", &file::Path::IsRelative)
      .def("__repr__", [](const file::Path& s) -> std::string {
        fxstring<64> fxs;
        fxs.format("Path(%s)", s.c_str());
        return fxs.c_str();
      });

  /////////////////////////////////////////////////////////////////////////////////
  py::class_<ork::Object>(module_core, "ork_Object") //
      .def("clazz", [](ork::Object* o) -> std::string {
        auto clazz = rtti::downcast<object::ObjectClass*>(o->GetClass());
        auto name  = clazz->Name();
        return name.c_str();
      });
  /////////////////////////////////////////////////////////////////////////////////
  pyinit_math(module_core);
  /////////////////////////////////////////////////////////////////////////////////
}; // PYBIND11_MODULE(_core, module_core) {

} // namespace ork
