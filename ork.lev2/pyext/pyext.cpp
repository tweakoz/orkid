#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx(py::module& module_lev2);
void pyinit_primitives(py::module& module_lev2);
void pyinit_scenegraph(py::module& module_lev2);
void pyinit_meshutil(py::module& module_lev2);

void ClassInit();
void GfxInit(const std::string& gfxlayer);
} // namespace ork::lev2

class Lev2PythonApplication : public ork::Application {
  RttiDeclareConcrete(Lev2PythonApplication, ork::Application);
};

void Lev2PythonApplication::Describe() {
}

INSTANTIATE_TRANSPARENT_RTTI(Lev2PythonApplication, "Lev2PythonApplication");

////////////////////////////////////////////////////////////////////////////////

void lev2appinit() {
  ork::SetCurrentThreadName("main");

  static Lev2PythonApplication the_app;
  ApplicationStack::Push(&the_app);

  int argc      = 1;
  char* argv[1] = {"python3"};

  static ork::lev2::StdFileSystemInitalizer filesysteminit(argc, argv);

  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
  ork::lev2::FontMan::GetRef();
}

////////////////////////////////////////////////////////////////////////////////

void lev2apppoll() {
  while (ork::opq::mainSerialQueue()->Process()) {
  }
}

////////////////////////////////////////////////////////////////////////////////

PYBIND11_MODULE(lev2, module_lev2) {

  module_lev2.attr("__name__") = "ork.lev2";

  //////////////////////////////////////////////////////////////////////////////
  module_lev2.doc() = "Orkid Lev2 Library (graphics,audio,vr,input,etc..)";
  //////////////////////////////////////////////////////////////////////////////
  module_lev2.def("lev2appinit", &lev2appinit);
  module_lev2.def("lev2apppoll", &lev2apppoll);
  //////////////////////////////////////////////////////////////////////////////
  pyinit_gfx(module_lev2);
  pyinit_primitives(module_lev2);
  pyinit_scenegraph(module_lev2);
  pyinit_meshutil(module_lev2);
  //////////////////////////////////////////////////////////////////////////////

  //////////////////////////////////////////////////////////////////////////////
};
