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

  type_codec->registerStdCodec<platform_ptr_t>(platform_type);

  auto device_type = py::class_<Device, device_ptr_t>(module_cl, "Device")
      .def_property_readonly("name", [](device_ptr_t the_device) -> std::string {
        return the_device->_name;
      })
      .def_property_readonly("context", [](device_ptr_t the_device) -> context_ptr_t {
        return the_device->_context;
      })
      .def_property_readonly("properties", [](device_ptr_t the_device) -> varmap::varmap_ptr_t {
        return the_device->_properties;
      });

  type_codec->registerStdCodec<device_ptr_t>(device_type);

  auto context_type = py::class_<Context, context_ptr_t>(module_cl, "Context")
      .def("createBuffer", [](context_ptr_t the_context, crcstring_ptr_t usage, size_t size) -> buffer_ptr_t {
        auto usage_enum = BufferUsage(usage->hashed());
        auto hpconfig = HostPointerConfig::MAP_TO_HOST_PTR;
        return the_context->createBuffer(usage_enum, hpconfig, size, nullptr);
      })
      .def("createBufferWithDataBlock", [](context_ptr_t the_context, crcstring_ptr_t usage, crcstring_ptr_t hpconfig, datablock_ptr_t dblock) -> buffer_ptr_t {
        auto usage_enum = BufferUsage(usage->hashed());
        auto hpconfig_enum = HostPointerConfig(hpconfig->hashed());
        size_t size = dblock->length();
        return the_context->createBuffer(usage_enum, hpconfig_enum, size, (void*) dblock->data());
      });

  type_codec->registerStdCodec<context_ptr_t>(context_type);

  auto buffer_type = py::class_<Buffer, buffer_ptr_t>(module_cl, "Buffer");

  type_codec->registerStdCodec<buffer_ptr_t>(buffer_type);

  auto kernel_type = py::class_<Kernel, kernel_ptr_t>(module_cl, "Kernel");

  type_codec->registerStdCodec<kernel_ptr_t>(kernel_type);

  module_cl.def("default_platform", [](py::kwargs kwargs) -> platform_ptr_t {
    auto g = get_globals();
    auto platforms = g->_platforms;
    if (platforms.size() == 0) {
      printf("OPENCL: no platforms\n");
      return nullptr;
    }
    return platforms[0];
  });
  
}

} //namespace ork {
