////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
    //   void set(fvec3 pos, fvec3 nrm, fvec3 bin, fvec2 uv, fvec4 col);

    .def(py::init<>())
    .def(py::init<>([](py::kwargs kwargs) -> vertex_ptr_t {
      auto v = std::make_shared<vertex>();
      for (auto item : kwargs) {
        auto key = py::cast<std::string>(item.first);
        if (key == "position") {
          v->mPos = fvec3_to_dvec3(py::cast<fvec3>(item.second));
        }
        else if (key == "normal") {
          v->mNrm = fvec3_to_dvec3(py::cast<fvec3>(item.second));
        }
        else if (key == "color0") {
          v->mCol[0] = py::cast<fvec4>(item.second);
        }
        else if (key == "uv0") {
          v->mUV[0].mMapTexCoord = py::cast<fvec2>(item.second);
        }
        else if (key == "binormal0") {
          v->mUV[0].mMapBiNormal = py::cast<fvec3>(item.second);
        }
        else if (key == "tangent0") {
          v->mUV[0].mMapTangent = py::cast<fvec3>(item.second);
        }
        else if (key == "uvc0") {
          v->mUV[0] = py::cast<uvmapcoord>(item.second);
        }
        else{
          OrkAssert(false);
        }
      }
      return v;
    }))
    .def_property_readonly("position", [](vertex_ptr_t vtx) -> dvec3 {            
      return vtx->mPos;
    })
    .def_property_readonly("normal", [](vertex_ptr_t vtx) -> dvec3 {            
      return vtx->mNrm;
    })
    .def_property_readonly("poolindex", [](vertex_ptr_t vtx) -> uint32_t {            
      return vtx->_poolindex;
    })
    .def_property_readonly("hash", [](vertex_ptr_t vtx) -> U64 {            
      return vtx->hash();
    })
    .def("uvc", [](vertex_ptr_t vtx, int iuvc) -> uvmapcoord {            
      OrkAssert(iuvc<vertex::kmaxuvs);
      return vtx->mUV[iuvc];
    })
    .def("color", [](vertex_ptr_t vtx, int ic) -> fvec4 {            
      OrkAssert(ic<vertex::kmaxcolors);
      return vtx->mCol[ic];
    })
    .def("__repr__", [](vertex_ptr_t v) -> std::string {
      std::string rval = FormatString("Vertex<%p>\n", (void*)v.get());
      rval += FormatString("  position<%g %g %g>\n", v->mPos.x, v->mPos.y,  v->mPos.z);
      rval += FormatString("  normal<%g %g %g>\n", v->mNrm.x, v->mNrm.y,  v->mNrm.z);
      rval += FormatString("  uvc0.uv<%g %g>\n", v->mUV[0].mMapTexCoord.x, v->mUV[0].mMapTexCoord.y  );
      rval += FormatString("  uvc0.bin<%g %g %g>\n", v->mUV[0].mMapBiNormal.x, v->mUV[0].mMapBiNormal.y, v->mUV[0].mMapBiNormal.z  );
      rval += FormatString("  uvc0.tan<%g %g %g>\n", v->mUV[0].mMapTangent.x, v->mUV[0].mMapTangent.y, v->mUV[0].mMapTangent.z  );
      rval += FormatString("  clr0<%g %g %g %g>\n", v->mCol[0].x, v->mCol[0].y, v->mCol[0].z, v->mCol[0].w  );
      rval += FormatString("  hash<0x%zx>\n", v->hash() );
      rval += FormatString("  poolindex<%d>\n", v->_poolindex );
      return rval;
    });
  type_codec->registerStdCodec<vertex_ptr_t>(vertex_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto uvc_type = py::class_<uvmapcoord>(module_meshutil, "UvMapCoord")
    .def_property("uv", [](const uvmapcoord& uvc) -> fvec2 {            
      return uvc.mMapTexCoord;
    },
    [](uvmapcoord& uvc, fvec2 uv) {            
      uvc.mMapTexCoord = uv;
    })
    .def_property("binormal", [](const uvmapcoord& uvc) -> fvec3 {            
      return uvc.mMapBiNormal;
    },
    [](uvmapcoord& uvc, fvec3 bin) {            
      uvc.mMapBiNormal = bin;
    })
    .def_property("tangent", [](const uvmapcoord& uvc) -> fvec3 {            
      return uvc.mMapTangent;
    },
    [](uvmapcoord& uvc, fvec3 tan) {            
      uvc.mMapTangent = tan;
    });
  type_codec->registerStdCodec<uvmapcoord>(uvc_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto edge_type = py::class_<edge,edge_ptr_t>(module_meshutil, "Edge").def(py::init<>())
    .def_property_readonly("numConnectedPolys", [](edge_ptr_t e) -> int {     
      auto sub = e->submesh();   
      auto conpolys = sub->connectedPolys(e);    
      return conpolys.size();
    })
    .def_property_readonly("connectedPolys", [](edge_ptr_t e) -> py::list {            
      py::list pyl;
      auto sub = e->submesh();   
      auto conpolys = sub->connectedPolys(e);    
      for( int c : conpolys ){
        pyl.append(c);
      }
      return pyl;
    })
    .def_property_readonly("vertices", [](edge_ptr_t e) -> py::list {            
      py::list pyl;
      pyl.append(e->_vertexA);
      pyl.append(e->_vertexB);
      return pyl;
    })
    .def_property_readonly("hash", [](edge_ptr_t e) -> uint64_t {            
      return e->hash();
    })
    .def("__repr__", [](edge_ptr_t e) -> std::string {
      return FormatString("edge[%d->%d]", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    });
  type_codec->registerStdCodec<edge_ptr_t>(edge_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto half_edge_type = py::class_<HalfEdge,halfedge_ptr_t>(module_meshutil, "HalfEdge").def(py::init<>())
    /*.def_property_readonly("numConnectedPolys", [](halfedge_ptr_t e) -> int {     
      auto sub = e->submesh();   
      auto conpolys = sub->connectedPolys(e);    
      return conpolys.size();
    })*/
   /* .def_property_readonly("connectedPolys", [](halfedge_ptr_t e) -> py::list {            
      py::list pyl;
      auto sub = e->submesh();   
      auto conpolys = sub->connectedPolys(e);    
      for( int c : conpolys ){
        pyl.append(c);
      }
      return pyl;
    })*/
    .def_property_readonly("polygon", [](halfedge_ptr_t e) -> poly_ptr_t { 
        return e->_polygon;
    })           
    //.def_property_readonly("twin", [](halfedge_ptr_t e) -> halfedge_ptr_t { 
      //  return e->_twin;
    //})           
    //.def_property_readonly("next", [](halfedge_ptr_t e) -> halfedge_ptr_t { 
      //  return e->_next;
    //})           
    .def_property_readonly("vertices", [](halfedge_ptr_t e) -> py::list {            
      py::list pyl;
      pyl.append(e->_vertexA);
      pyl.append(e->_vertexB);
      return pyl;
    })
    .def_property_readonly("hash", [](halfedge_ptr_t e) -> uint64_t {            
      return e->hash();
    })
    .def("__repr__", [](halfedge_ptr_t e) -> std::string {
      return FormatString("HalfEdge[%d->%d]", e->_vertexA->_poolindex, e->_vertexB->_poolindex);
    });
  type_codec->registerStdCodec<halfedge_ptr_t>(half_edge_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto poly_type = py::class_<Polygon,poly_ptr_t>(module_meshutil, "Polygon")
    .def_property_readonly("numSides", [](poly_ptr_t p) -> int {            
      return p->numSides();
    })
    .def_property_readonly("numVertices", [](poly_ptr_t p) -> int {            
      return p->numVertices();
    })
    .def_property_readonly("normal", [](poly_ptr_t p) -> fvec3 {            
      return dvec3_to_fvec3(p->computeNormal());
    })
    .def_property_readonly("center", [](poly_ptr_t p) -> fvec3 {            
      return dvec3_to_fvec3(p->computeCenter().mPos);
    })
    .def_property_readonly("area", [](poly_ptr_t p) -> double {            
      return p->computeArea(dmtx4());
    })
    .def_property_readonly("plane", [](poly_ptr_t p) -> fplane3_ptr_t {            
      auto pl = std::make_shared<fplane3>(dplane3_to_fplane3(p->computePlane()));
      return pl;
    })
    .def_property_readonly("edges", [](poly_ptr_t p) -> py::list {  
      py::list pyl;          
      for( auto e : p->edges() ){
        pyl.append(e);
      }
      return pyl;
    })
    .def_property_readonly("vertices", [](poly_ptr_t p) -> py::list {            
      py::list pyl;          
      p->visitVertices([&](vertex_ptr_t v){
        pyl.append(v);
      });
      return pyl;
    })
    .def_property_readonly("indices", [](poly_ptr_t p) -> py::list {            
      py::list pyl;          
      p->visitVertices([&](vertex_ptr_t v){
        pyl.append(v->_poolindex);
      });
      return pyl;
    })
    .def("vertexIndex", [](poly_ptr_t p, int i) -> int {            
      return p->vertexID(i);
    })
    .def("vertex", [](poly_ptr_t p, int i) -> vertex_ptr_t {            
      return p->vertex(i);
    })
    .def("__repr__", [](poly_ptr_t p) -> std::string {
      std::string rval = FormatString("Poly<%p> [ ", (void*)p.get());
      p->visitVertices([&](vertex_ptr_t v){
        rval += FormatString("%d ", v->_poolindex);
      });
      rval += FormatString("]" );
      return rval;
    });
  type_codec->registerStdCodec<poly_ptr_t>(poly_type);
  /////////////////////////////////////////////////////////////////////////////////
  auto merged_poly_type = py::class_<MergedPolygon,Polygon,merged_poly_ptr_t>(module_meshutil, "MergedPolygon");
  type_codec->registerStdCodec<merged_poly_ptr_t>(merged_poly_type);
  /////////////////////////////////////////////////////////////////////////////////
}
}
