////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
using namespace meshutil;
void pyinit_meshutil_component(py::module& module_meshutil) {
  auto type_codec = python::TypeCodec::instance();

  /////////////////////////////////////////////////////////////////////////////////
  auto vtxpool_type = py::class_<vertexpool,vertexpool_ptr_t>(module_meshutil, "VertexPool").def(py::init<>());
  type_codec->registerStdCodec<vertexpool_ptr_t>(vtxpool_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto poly_type = py::class_<poly,poly_ptr_t>(module_meshutil, "Poly");
  type_codec->registerStdCodec<poly_ptr_t>(poly_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto edge_type = py::class_<edge,edge_ptr_t>(module_meshutil, "Edge").def(py::init<>());
  type_codec->registerStdCodec<edge_ptr_t>(vtxpool_type);
}
}
