////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx(py::module& module_lev2);
void pyinit_primitives(py::module& module_lev2);
void pyinit_scenegraph(py::module& module_lev2);
void pyinit_meshutil(py::module& module_lev2);
void pyinit_gfx_qtez(py::module& module_lev2);

void ClassInit();
void GfxInit(const std::string& gfxlayer);
} // namespace ork::lev2

////////////////////////////////////////////////////////////////////////////////

orkezapp_ptr_t lev2appinit() {
  ork::SetCurrentThreadName("main");

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwritable-strings"

  int argc      = 1;
  char* argv[1] = {"python3"};

#pragma GCC diagnostic pop

  static auto init_data = std::make_shared<AppInitData>(argc,argv);

  auto vars = *init_data->parse();

  auto ezapp = OrkEzApp::create(init_data);


  ork::lev2::ClassInit();
  ork::rtti::Class::InitializeClasses();
  ork::lev2::GfxInit("");
  ork::lev2::FontMan::GetRef();

  return ezapp;
}

////////////////////////////////////////////////////////////////////////////////

void lev2apppoll() {
  while (ork::opq::mainSerialQueue()->Process()) {
  }
}

////////////////////////////////////////////////////////////////////////////////

PYBIND11_MODULE(_lev2, module_lev2) {
  // module_lev2.attr("__name__") = "lev2";

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
  pyinit_gfx_qtez(module_lev2);
  //////////////////////////////////////////////////////////////////////////////
};
