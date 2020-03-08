#pragma once
#include <ork/lev2/gfx/submesh.h>
#include <ork/math/frustum.h>

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////

struct FrustumPrimitive {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context) {

    using namespace MeshUtil;

    submesh submeshQuads;
    submesh submeshTris;

    float N = -_size * 0.5f;
    float P = +_size * 0.5f;

    submeshQuads.addQuad(
        _frustum.mNearCorners[3],
        _frustum.mNearCorners[2],
        _frustum.mNearCorners[1],
        _frustum.mNearCorners[0], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        _colorFront);
    submeshQuads.addQuad(
        _frustum.mFarCorners[0],
        _frustum.mFarCorners[1],
        _frustum.mFarCorners[2],
        _frustum.mFarCorners[3], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        _colorBack);
    submeshQuads.addQuad(
        _frustum.mNearCorners[1],
        _frustum.mFarCorners[1],
        _frustum.mFarCorners[0],
        _frustum.mNearCorners[0], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        _colorTop);
    submeshQuads.addQuad(
        _frustum.mNearCorners[3],
        _frustum.mFarCorners[3],
        _frustum.mFarCorners[2],
        _frustum.mNearCorners[2], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        _colorBottom);
    submeshQuads.addQuad(
        _frustum.mNearCorners[0],
        _frustum.mFarCorners[0],
        _frustum.mFarCorners[3],
        _frustum.mNearCorners[3], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        _colorLeft);
    submeshQuads.addQuad(
        _frustum.mNearCorners[2],
        _frustum.mFarCorners[2],
        _frustum.mFarCorners[1],
        _frustum.mNearCorners[1], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        _colorRight);

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

  ork::Frustum _frustum;
  MeshUtil::PrimitiveV12N12B12T8C4 _primitive;
};

} // namespace ork::lev2::primitives
