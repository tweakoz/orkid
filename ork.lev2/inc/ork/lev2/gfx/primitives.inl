#pragma once
#include <ork/lev2/gfx/gfxprimitives.h>

namespace ork::lev2::primitives {

typedef SVtxV12N12B12T8C4 vtx_t;

struct Cube {
  float _size = 0.0f;

  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorFront;
  fvec4 _colorBack;
  fvec4 _colorLeft;
  fvec4 _colorRight;

  inline void draw(Context* context, DynamicVertexBuffer<vtx_t>& vtxbuf) {

    VtxWriter<vtx_t> vwriter;

    vwriter.Lock(context, &vtxbuf,
                 24); // reserve 24 verts (4 quads, or 8 triangles)

    auto writevtx = [&](float x, float y, float z, uint32_t c) { vwriter.AddVertex(vtx_t(x, y, z, 0, 0, 0, 0, c)); };

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    writevtx(N, N, N, _colorFront);
    writevtx(P, N, N, _colorFront);
    writevtx(N, P, N, _colorFront);
    writevtx(N, P, N, _colorFront);
    writevtx(P, N, N, _colorFront);
    writevtx(P, P, N, _colorFront);

    writevtx(N, N, N, _colorLeft);
    writevtx(N, N, P, _colorLeft);
    writevtx(N, P, N, _colorLeft);
    writevtx(N, P, N, _colorLeft);
    writevtx(N, N, P, _colorLeft);
    writevtx(N, P, P, _colorLeft);

    writevtx(P, P, P, _colorBack);
    writevtx(N, P, P, _colorBack);
    writevtx(P, N, P, _colorBack);
    writevtx(P, N, P, _colorBack);
    writevtx(N, P, P, _colorBack);
    writevtx(N, N, P, _colorBack);

    writevtx(P, P, P, _colorRight);
    writevtx(P, P, N, _colorRight);
    writevtx(P, N, P, _colorRight);
    writevtx(P, N, P, _colorRight);
    writevtx(P, P, N, _colorRight);
    writevtx(P, N, N, _colorRight);

    writevtx(P, P, P, _colorTop);
    writevtx(P, P, N, _colorTop);
    writevtx(N, P, P, _colorTop);
    writevtx(N, P, P, _colorTop);
    writevtx(P, P, N, _colorTop);
    writevtx(N, P, N, _colorTop);

    writevtx(N, N, N, _colorBottom);
    writevtx(N, N, P, _colorBottom);
    writevtx(P, N, N, _colorBottom);
    writevtx(P, N, N, _colorBottom);
    writevtx(N, N, P, _colorBottom);
    writevtx(P, N, P, _colorBottom);

    vwriter.UnLock(context);

    auto gbi = context->GBI(); // GeometryBuffer Interface
    gbi->DrawPrimitiveEML(vwriter, EPRIM_TRIANGLES);
  }
};

} // namespace ork::lev2::primitives
