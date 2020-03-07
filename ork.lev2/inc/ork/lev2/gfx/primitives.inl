#pragma once
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/submesh.h>

namespace ork::lev2::primitives {

typedef SVtxV12N12B12T8C4 vtx_t;
typedef StaticIndexBuffer<uint16_t> idxbuf_t;

//////////////////////////////////////////////////////////////////////////////

struct Cube {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context, DynamicVertexBuffer<vtx_t>& vtxbuf) {

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    _submeshQuads.addQuad(
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec3(P, P, P),
        fvec3(N, P, P), //
        fvec2(0.0f, 0.0f),
        fvec2(0.25f, 0.5f),
        _colorFront);

    _submeshQuads.addQuad(
        fvec3(P, N, P),
        fvec3(P, N, N),
        fvec3(P, P, N),
        fvec3(P, P, P), //
        fvec2(0.25f, 0.0f),
        fvec2(0.5f, 0.5f),
        _colorRight);

    _submeshQuads.addQuad(
        fvec3(P, N, N),
        fvec3(N, N, N),
        fvec3(N, P, N),
        fvec3(P, P, N), //
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorBack);

    _submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(N, N, P),
        fvec3(N, P, P),
        fvec3(N, P, N), //
        fvec2(0.75f, 0.0f),
        fvec2(1.0f, 0.5f),
        _colorLeft);

    _submeshQuads.addQuad(
        fvec3(N, P, N),
        fvec3(N, P, P),
        fvec3(P, P, P),
        fvec3(P, P, N), //
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        _colorTop);

    _submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(P, N, N), //
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        _colorBottom);

    _submeshQuads.triangulate(_submeshTris);

    _primitive = _submeshTris.generatePrimitive_V12N12B12T8C4(context);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void draw(Context* context) {
    auto& VB = _primitive->_vertexBuffer;
    auto& IB = _primitive->_indexBuffer;
    context->GBI()->DrawIndexedPrimitiveEML(VB, IB, EPRIM_TRIANGLES);
  }

  //////////////////////////////////////////////////////////////////////////////

  float _size = 0.0f;

  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorFront;
  fvec4 _colorBack;
  fvec4 _colorLeft;
  fvec4 _colorRight;

  MeshUtil::submesh _submeshQuads;
  MeshUtil::submesh _submeshTris;

  std::shared_ptr<MeshUtil::PrimitiveV12N12B12T8C4> _primitive;

}; // namespace ork::lev2::primitives

} // namespace ork::lev2::primitives
