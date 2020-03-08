////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/submesh.h>

namespace ork::MeshUtil {

PrimitiveV12N12B12T8C4::PrimitiveV12N12B12T8C4() {
}

////////////////////////////////////////////////////////////////////////////////

void PrimitiveV12N12B12T8C4::fromSubMesh(const submesh& submesh, std::shared_ptr<vtxbuf_t> vtxbuf, lev2::Context* context) {
  const auto& vpool = submesh.RefVertexPool();
  int numverts      = vpool.GetNumVertices();
  int inumpolys     = submesh.GetNumPolys(3);
  int numidcs       = inumpolys * 3;

  _vertexBuffer = vtxbuf;
  _indexBuffer  = std::make_shared<idxbuf_t>(numidcs);

  _writer.Lock(context, _vertexBuffer.get(), numverts);
  for (int i = 0; i < numverts; i++) {
    const auto& inpvtx = vpool.GetVertex(i);
    const auto& pos    = inpvtx.mPos;
    const auto& nrm    = inpvtx.mNrm;
    const auto& uv     = inpvtx.mUV[0].mMapTexCoord;
    const auto& bin    = inpvtx.mUV[0].mMapBiNormal;
    const auto& col    = inpvtx.mCol[0];
    _writer.AddVertex(lev2::SVtxV12N12B12T8C4(pos, nrm, bin, uv, col.GetABGRU32()));
  }
  _writer.UnLock(context);

  ///////////////////////////////////////////////
  // submesh indices -> index buffer
  ///////////////////////////////////////////////
  auto pidxout = (uint16_t*)context->GBI()->LockIB(*_indexBuffer.get(), 0, numidcs);
  int index    = 0;
  for (int p = 0; p < inumpolys; p++) {
    const auto& poly = submesh.RefPoly(p);
    pidxout[index++] = (uint16_t)poly.miVertices[0];
    pidxout[index++] = (uint16_t)poly.miVertices[1];
    pidxout[index++] = (uint16_t)poly.miVertices[2];
  }
  context->GBI()->UnLockIB(*_indexBuffer.get());
}

////////////////////////////////////////////////////////////////////////////////

void PrimitiveV12N12B12T8C4::fromSubMesh(const submesh& submesh, lev2::Context* context) {
  const auto& vpool = submesh.RefVertexPool();
  int numverts      = vpool.GetNumVertices();
  _vertexBuffer     = std::make_shared<vtxbuf_t>(numverts, 0, lev2::EPRIM_NONE);
  fromSubMesh(submesh, _vertexBuffer, context);
}

////////////////////////////////////////////////////////////////////////////////

void PrimitiveV12N12B12T8C4::draw(lev2::Context* context) const {
  auto& VB = *_vertexBuffer.get();
  auto& IB = *_indexBuffer.get();
  context->GBI()->DrawIndexedPrimitiveEML(VB, IB, lev2::EPRIM_TRIANGLES);
}

} // namespace ork::MeshUtil
