#pragma once
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/submesh.h>

namespace ork::lev2::primitives {

typedef SVtxV12N12B12T8C4 vtx_t;

struct Cube {

  inline void gpuInit(Context* context, DynamicVertexBuffer<vtx_t>& vtxbuf) {
    _vwriter.Lock(context, &vtxbuf, 36);

    auto writevtx = [&](float x, float y, float z, float u, float v, fvec4 c) {
      auto pos = fvec3(x, y, z);
      auto nrm = pos;
      auto bin = pos.Cross(fvec3(y, x, z));
      _vwriter.AddVertex(vtx_t(pos, nrm, bin, fvec2(u, v), c.GetABGRU32()));
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

    _vwriter.UnLock(context);
  }

  inline void draw(Context* context) {
    context->GBI()->DrawPrimitiveEML(_vwriter, EPRIM_TRIANGLES);
  }

  float _size = 0.0f;

  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorFront;
  fvec4 _colorBack;
  fvec4 _colorLeft;
  fvec4 _colorRight;
  VtxWriter<vtx_t> _vwriter;

}; // namespace ork::lev2::primitives

} // namespace ork::lev2::primitives
