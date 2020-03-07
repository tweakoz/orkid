#pragma once
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/submesh.h>

namespace ork::lev2::primitives {

typedef SVtxV12N12B12T8C4 vtx_t;
typedef StaticIndexBuffer<uint16_t> idxbuf_t;

struct Cube {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context, DynamicVertexBuffer<vtx_t>& vtxbuf) {

    ///////////////////////////////////////////////
    // merge vertices into vertex pool
    ///////////////////////////////////////////////

    std::vector<uint16_t> indices;
    auto writevtx = [&](float x, float y, float z, float u, float v, fvec4 c) {
      MeshUtil::vertex muvtx;
      muvtx.mPos                = fvec3(x, y, z);
      muvtx.mNrm                = muvtx.mPos;
      muvtx.mUV[0].mMapTexCoord = fvec2(u, v);
      muvtx.mUV[0].mMapBiNormal = muvtx.mPos.Cross(fvec3(y, x, z));
      muvtx.mCol[0]             = c;
      muvtx.miNumColors         = 1;
      muvtx.miNumUvs            = 1;
      indices.push_back(_submesh.MergeVertex(muvtx));
    };

    float v0 = 0.0f;
    float v1 = 0.5f;

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    float u0 = 0.0f;
    float u1 = 0.25f;

    writevtx(P, P, P, u1, v1, _colorFront);
    writevtx(N, P, P, u0, v1, _colorFront);
    writevtx(P, N, P, u1, v0, _colorFront);
    writevtx(P, N, P, u1, v0, _colorFront);
    writevtx(N, P, P, u0, v1, _colorFront);
    writevtx(N, N, P, u0, v0, _colorFront);

    u1 = 0.25f;
    u0 = 0.5f;

    writevtx(P, P, P, u1, v1, _colorRight);
    writevtx(P, N, P, u1, v0, _colorRight);
    writevtx(P, P, N, u0, v1, _colorRight);
    writevtx(P, P, N, u0, v1, _colorRight);
    writevtx(P, N, P, u1, v0, _colorRight);
    writevtx(P, N, N, u0, v0, _colorRight);

    u0 = 0.75f;
    u1 = 0.5f;

    writevtx(N, N, N, u0, v0, _colorBack);
    writevtx(N, P, N, u0, v1, _colorBack);
    writevtx(P, N, N, u1, v0, _colorBack);
    writevtx(P, N, N, u1, v0, _colorBack);
    writevtx(N, P, N, u0, v1, _colorBack);
    writevtx(P, P, N, u1, v1, _colorBack);

    u0 = 0.75f;
    u1 = 1.0f;

    writevtx(N, N, N, u0, v0, _colorLeft);
    writevtx(N, N, P, u1, v0, _colorLeft);
    writevtx(N, P, N, u0, v1, _colorLeft);
    writevtx(N, P, N, u0, v1, _colorLeft);
    writevtx(N, N, P, u1, v0, _colorLeft);
    writevtx(N, P, P, u1, v1, _colorLeft);

    u0 = 1.0f;
    u1 = 0.0f;
    v0 = 0.5f;
    v1 = 1.0f;

    writevtx(P, P, P, u1, v1, _colorTop);
    writevtx(P, P, N, u1, v0, _colorTop);
    writevtx(N, P, P, u0, v1, _colorTop);
    writevtx(N, P, P, u0, v1, _colorTop);
    writevtx(P, P, N, u1, v0, _colorTop);
    writevtx(N, P, N, u0, v0, _colorTop);

    writevtx(N, N, N, u0, v0, _colorBottom);
    writevtx(P, N, N, u1, v0, _colorBottom);
    writevtx(N, N, P, u0, v1, _colorBottom);
    writevtx(N, N, P, u0, v1, _colorBottom);
    writevtx(P, N, N, u1, v0, _colorBottom);
    writevtx(P, N, P, u1, v1, _colorBottom);

    ///////////////////////////////////////////////
    // vertex pool -> vertex buffer
    ///////////////////////////////////////////////

    const auto& vpool = _submesh.RefVertexPool();
    int numverts      = vpool.GetNumVertices();
    _vwriter.Lock(context, &vtxbuf, numverts);
    for (int i = 0; i < numverts; i++) {
      const auto& inpvtx = vpool.GetVertex(i);
      const auto& pos    = inpvtx.mPos;
      const auto& nrm    = inpvtx.mNrm;
      const auto& uv     = inpvtx.mUV[0].mMapTexCoord;
      const auto& bin    = inpvtx.mUV[0].mMapBiNormal;
      const auto& col    = inpvtx.mCol[0];
      _vwriter.AddVertex(vtx_t(pos, nrm, bin, uv, col.GetABGRU32()));
    }
    _vwriter.UnLock(context);

    ///////////////////////////////////////////////
    // submesh indices -> index buffer
    ///////////////////////////////////////////////

    int numidcs  = indices.size();
    _indexbuffer = std::make_shared<idxbuf_t>(numidcs);
    auto pidxout = (uint16_t*)context->GBI()->LockIB(*_indexbuffer.get(), 0, numidcs);
    for (int i = 0; i < numidcs; i++) {
      pidxout[i] = (uint16_t)indices[i];
    }
    context->GBI()->UnLockIB(*_indexbuffer.get());
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void draw(Context* context) {
    auto& VB = *_vwriter.mpVB;
    auto& IB = *_indexbuffer.get();
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
  VtxWriter<vtx_t> _vwriter;
  std::shared_ptr<idxbuf_t> _indexbuffer;
  MeshUtil::submesh _submesh;

}; // namespace ork::lev2::primitives

} // namespace ork::lev2::primitives
