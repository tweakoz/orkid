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
  auto vtxpool_type = py::class_<vertexpool,vertexpool_ptr_t>(module_meshutil, "VertexPool") //
      .def(py::init<>())
      .def_property_readonly("orderedVertices", [](vertexpool_ptr_t vpool) -> py::list {            
          py::list pyl;
          for( auto v : vpool->_orderedVertices ){
            pyl.append(v);
          }
          return pyl;
          });
  type_codec->registerStdCodec<vertexpool_ptr_t>(vtxpool_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto vertex_type = py::class_<vertex,vertex_ptr_t>(module_meshutil, "Vertex") //
    .def(py::init<>())
    .def_property_readonly("position", [](vertex_ptr_t vtx) -> fvec3 {            
      return vtx->mPos;
    })
    .def_property_readonly("normal", [](vertex_ptr_t vtx) -> fvec3 {            
      return vtx->mNrm;
    })
    .def("uvc", [](vertex_ptr_t vtx, int iuvc) -> uvmapcoord {            
      OrkAssert(iuvc<vertex::kmaxuvs);
      return vtx->mUV[iuvc];
    })
    .def("color", [](vertex_ptr_t vtx, int ic) -> fvec4 {            
      OrkAssert(ic<vertex::kmaxcolors);
      return vtx->mCol[ic];
    });
  type_codec->registerStdCodec<vertex_ptr_t>(vertex_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uvc_type = py::class_<uvmapcoord>(module_meshutil, "UvMapCoord")
    .def_property_readonly("uv", [](const uvmapcoord& uvc) -> fvec2 {            
      return uvc.mMapTexCoord;
    })
    .def_property_readonly("binormal", [](const uvmapcoord& uvc) -> fvec3 {            
      return uvc.mMapBiNormal;
    })
    .def_property_readonly("tangent", [](const uvmapcoord& uvc) -> fvec3 {            
      return uvc.mMapTangent;
    });
  type_codec->registerStdCodec<uvmapcoord>(uvc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto edge_type = py::class_<edge,edge_ptr_t>(module_meshutil, "Edge").def(py::init<>());
  type_codec->registerStdCodec<edge_ptr_t>(vtxpool_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto poly_type = py::class_<poly,poly_ptr_t>(module_meshutil, "Poly");
  type_codec->registerStdCodec<poly_ptr_t>(poly_type);
}
}
