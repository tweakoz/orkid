////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/gfxvtxbuf.inl>
#include <pybind11/numpy.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
//
void pyinit_gfx_buffers(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  /////////////////////////////////////////////////////////////////////////////////
  auto idxbuf_t = py::class_<IndexBufferBase,idxbufferbase_ptr_t>(module_lev2, "IndexBufferBase");
  idxbuf_t.def_property_readonly("numIndices", [](idxbufferbase_ptr_t ib) -> int { return ib->GetNumIndices(); });
  idxbuf_t.def_property_readonly("indexSize", [](idxbufferbase_ptr_t ib) -> size_t { return ib->indexSize(); });
  idxbuf_t.def_property_readonly("isStatic", [](idxbufferbase_ptr_t ib) -> bool { return ib->IsStatic(); });
  type_codec->registerStdCodec<idxbufferbase_ptr_t>(idxbuf_t);
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def(
      "createIndexBufferStaticU16",
      [](ctx_t ctx, py::array_t<uint16_t> indices) -> idxbufferbase_ptr_t {
        size_t count = indices.size();
        idxbufferbase_ptr_t ib = std::make_shared<StaticIndexBuffer<uint16_t>>(count);
        auto dst_indices = (uint16_t*) ctx->GBI()->LockIB(*ib, 0, count);
        auto src_indices = (const uint16_t*)indices.data();

        for(size_t i = 0; i < count; i++) {
          dst_indices[i] = src_indices[i];
        }
        ctx->GBI()->UnLockIB(*ib);
        return ib;
      });
  /////////////////////////////////////////////////////////////////////////////////
      auto vtxbuf_t = py::class_<VertexBufferBase,vtxbufferbase_ptr_t>(module_lev2, "VertexBufferBase");
  vtxbuf_t.def_property_readonly("numVertices", [](vtxbufferbase_ptr_t vb) -> int { return vb->GetNumVertices(); });
  vtxbuf_t.def_property_readonly("vertexSize", [](vtxbufferbase_ptr_t vb) -> size_t { return vb->GetVtxSize(); });
  vtxbuf_t.def_property_readonly("isStatic", [](vtxbufferbase_ptr_t vb) -> bool { return vb->IsStatic(); });
  type_codec->registerStdCodec<vtxbufferbase_ptr_t>(vtxbuf_t);
  /////////////////////////////////////////////////////////////////////////////////
  py::class_<vtxa_t>(module_lev2, "VtxV12N12B12T8C4")
      .def(py::init<fvec3, fvec3, fvec3, fvec2, uint32_t>())
      .def_static(
          "staticBuffer",
          [](size_t size) -> vb_static_vtxa_t //
          { return vb_static_vtxa_t(size, 0); });
  /////////////////////////////////////////////////////////////////////////////////
  //py::class_<vb_static_vtxa_t, VertexBufferBase>(module_lev2, "VtxV12N12B12T8C4_StaticBuffer");
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def(
    "createVertexBufferStaticV12T8",
    [](ctx_t ctx, py::array_t<float> vertices) -> vtxbufferbase_ptr_t {
      AABox bbox;
      size_t num_floats = vertices.size();
      OrkAssert(num_floats % 5 == 0);
      size_t count = num_floats/5;
      auto vb = std::make_shared<StaticVertexBuffer<VtxV12T8>>(count,0);
      auto dst_vertices = (VtxV12T8*) ctx->GBI()->LockVB(*vb, 0, count);
      auto src_vertices = (const float*)vertices.data();
      vb->miNumVerts = count;
      for(size_t i = 0; i < count; i++) {
        size_t j = i * 5;
        auto& dst_vertex = dst_vertices[i];
        dst_vertex.pos.x = src_vertices[j+0];
        dst_vertex.pos.y = src_vertices[j+1];
        dst_vertex.pos.z = src_vertices[j+2];
        dst_vertex.uv0.x = src_vertices[j+3];
        dst_vertex.uv0.y = src_vertices[j+4];
        bbox.Grow(dst_vertex.pos);
      }
      vb->_aabb = bbox;
      ctx->GBI()->UnLockVB(*vb);
      return vb;
    });
  /////////////////////////////////////////////////////////////////////////////////
  //py::class_<vb_static_vtxa_t, VertexBufferBase>(module_lev2, "VtxV12N12B12T8C4_StaticBuffer");
  /////////////////////////////////////////////////////////////////////////////////
  module_lev2.def(
    "createVertexBufferStatic",
    [](crcstring_ptr_t fmt, size_t count) -> vtxbufferbase_ptr_t {
      auto efmt = EVtxStreamFormat(fmt->hashed());
      auto vb = VertexBufferBase::CreateVertexBuffer(efmt,count,false);
      vb->miNumVerts = count;
      return vb;
    });
    module_lev2.def(
      "createVertexBufferDynamic",
      [](crcstring_ptr_t fmt, size_t count) -> vtxbufferbase_ptr_t {
        auto efmt = EVtxStreamFormat(fmt->hashed());
        auto vb = VertexBufferBase::CreateVertexBuffer(efmt,count,true);
        vb->miNumVerts = count;
        return vb;
      });
    /////////////////////////////////////////////////////////////////////////////////

  PYBIND11_NUMPY_DTYPE(VtxV12C4, x, y, z, color);
  PYBIND11_NUMPY_DTYPE(_VtxV12T8, x, y, z, u, v);
  PYBIND11_NUMPY_DTYPE(SVtxVU32, _data);
  PYBIND11_NUMPY_DTYPE(SVtxVU32Inst, _instanceId);

}
} // namespace ork::lev2 {