///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/opencl/opencl.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
using namespace opencl;
///////////////////////////////////////////////////////////////////////////////
static globals_ptr_t get_globals(){
  static globals_ptr_t _globals = std::make_shared<Globals>();
  return _globals;
}

///////////////////////////////////////////////////////////////////////////////
void pyinit_opencl(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto module_cl = module_core.def_submodule("opencl", "Orkid OpenCL context");

  auto platform_type = py::class_<Platform, platform_ptr_t>(module_cl, "Platform")
                           .def_property_readonly(
                               "devices", //
                               [](platform_ptr_t the_platform) -> std::vector<device_ptr_t> {
                                 return the_platform->_devices;
                               })
                               .def_property_readonly("name", [](platform_ptr_t the_platform) -> std::string {
                                 return the_platform->_name;
                               });


  module_cl.def("default_platform", [](py::kwargs kwargs) -> platform_ptr_t {
    auto g = get_globals();
    auto platforms = g->_platforms;
    if (platforms.size() == 0) {
      return nullptr;
    }
    return platforms[0];
  });
  
}

} //namespace ork {
