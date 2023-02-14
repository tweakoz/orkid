////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/camera/cameradata.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_renderer(py::module& module_lev2) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto rcfd_type_t = py::class_<RenderContextFrameData,rcfd_ptr_t>( module_lev2, //
    "RenderContextFrameData") //
    .def(py::init([](ctx_t& ctx) -> rcfd_ptr_t { //
      return std::make_shared<RenderContextFrameData>(ctx.get());
    }))
    .def_property("cimpl",
      [](rcfd_ptr_t the_rcfd) -> compositorimpl_ptr_t {
        return the_rcfd->_cimpl;
      },
      [](rcfd_ptr_t the_rcfd, compositorimpl_ptr_t c){
        the_rcfd->_cimpl = c;
      }
    )
    .def("setRenderingModel", 
        [](rcfd_ptr_t the_rcfd, std::string rendermodel) { //
          auto as_crc = CrcString(rendermodel.c_str());
          the_rcfd->_renderingmodel = (uint32_t) as_crc._hashed;
        }
    );
  type_codec->registerStdCodec<rcfd_ptr_t>(rcfd_type_t);
  /////////////////////////////////////////////////////////////////////////////////
  auto rcid_type_t = py::class_<RenderContextInstData,rcid_ptr_t>(module_lev2, //
    "RenderContextInstData") //
    .def(py::init([](rcfd_ptr_t the_rcfd) -> rcid_ptr_t { //
      return RenderContextInstData::create(the_rcfd);
    }))
    .def("fxinst", //
        [](rcid_ptr_t the_rcid, material_ptr_t mtl) -> fxinstance_ptr_t { //
          auto cache = mtl->fxInstanceCache();
          return cache->findfxinst(*the_rcid);
        }
    )
    .def("genMatrix", //
        [](rcid_ptr_t the_rcid, py::object method) { //
          auto py_callback = method.cast<pybind11::object>();
          the_rcid->_genMatrix = [py_callback]() -> fmtx4 {
            py::gil_scoped_acquire acquire;
            py::object mtx_attempt = py_callback();
            printf ("YAY..\n");
            return mtx_attempt.cast<fmtx4>();
          };
        }
    )
    .def("forceTechnique", //
        [](rcid_ptr_t the_rcid, pyfxtechnique_ptr_t tek) { //
          the_rcid->forceTechnique(tek.get());
        }
    );
    /*.def_property("fxcache", 
        [](rcid_ptr_t the_rcid) -> fxinstancecache_constptr_t { //
          return the_rcid->_fx_instance_cache;
        },
        [](rcid_ptr_t the_rcid, fxinstancecache_constptr_t cache) { //
          the_rcid->_fx_instance_cache = cache;
        }
    )*/
  type_codec->registerStdCodec<rcid_ptr_t>(rcid_type_t);
}
} //namespace ork::lev2 {
