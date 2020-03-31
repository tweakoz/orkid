#pragma once
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////

struct CubePrimitive {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context) {

    using namespace meshutil;

    submesh submeshQuads;
    submesh submeshTris;

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    submeshQuads.addQuad(
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec3(P, P, P),
        fvec3(N, P, P), //
        fvec2(0.0f, 0.0f),
        fvec2(0.25f, 0.5f),
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
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorLeft);

    submeshQuads.addQuad(
        fvec3(N, P, N),
        fvec3(N, P, P),
        fvec3(P, P, P),
        fvec3(P, P, N), //
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorTop);

    submeshQuads.addQuad(
        fvec3(N, N, N),
        fvec3(P, N, N), //
        fvec3(N, N, P),
        fvec3(P, N, P),
        fvec2(0.0f, 0.5f),
        fvec2(1.0f, 1.5f),
        fvec2(0.5f, 0.0f),
        fvec2(0.75f, 0.5f),
        _colorBottom);

    submeshTriangulate(submeshQuads, submeshTris);

    _primitive.fromSubMesh(submeshTris, context);
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

  meshutil::PrimitiveV12N12B12T8C4 _primitive;
};

} // namespace ork::lev2::primitives
