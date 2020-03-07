#pragma once
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/submesh.h>

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////

struct Cube {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context) {

    MeshUtil::submesh submeshQuads;
    MeshUtil::submesh submeshTris;

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    submeshQuads.addQuad(
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec3(P, P, P),
        fvec3(N, P, P), //
        fvec2(0.0f, 0.0f),
        fvec2(0.25f, 0.5f),
        _colorFront);

    submeshQuads.addQuad(
        fvec3(P, N, P),
        fvec3(P, N, N),
        fvec3(P, P, N),
        fvec3(P, P, P), //
        fvec2(0.25f, 0.0f),
        fvec2(0.5f, 0.5f),
        _colorRight);

    submeshQuads.addQuad(
        fvec3(P, N, N),
        fvec3(N, N, N),
        fvec3(N, P, N),
        fvec3(P, P, N), //
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorBack);

    submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(N, N, P),
        fvec3(N, P, P),
        fvec3(N, P, N), //
        fvec2(0.75f, 0.0f),
        fvec2(1.0f, 0.5f),
        _colorLeft);

    submeshQuads.addQuad(
        fvec3(N, P, N),
        fvec3(N, P, P),
        fvec3(P, P, P),
        fvec3(P, P, N), //
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        _colorTop);

    submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(P, N, N), //
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        _colorBottom);

    submeshQuads.triangulate(submeshTris);

    _primitive.gpuInit(submeshTris, context);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void draw(Context* context) {
    _primitive.draw(context);
  }

  //////////////////////////////////////////////////////////////////////////////

  float _size = 0.0f;

  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorFront;
  fvec4 _colorBack;
  fvec4 _colorLeft;
  fvec4 _colorRight;

  MeshUtil::PrimitiveV12N12B12T8C4 _primitive;
};

} // namespace ork::lev2::primitives
