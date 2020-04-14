#pragma once
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/math/frustum.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

namespace ork::lev2::primitives {

//////////////////////////////////////////////////////////////////////////////

struct FrustumPrimitive {

  //////////////////////////////////////////////////////////////////////////////

  inline void gpuInit(Context* context) {

    using namespace meshutil;

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
        fvec2(1.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        fvec2(0.0f, 1.0f),
        _colorFront);
    submeshQuads.addQuad(
        _frustum.mFarCorners[0],
        _frustum.mFarCorners[1],
        _frustum.mFarCorners[2],
        _frustum.mFarCorners[3], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        fvec2(0.0f, 1.0f),
        _colorBack);
    submeshQuads.addQuad(
        _frustum.mNearCorners[1],
        _frustum.mFarCorners[1],
        _frustum.mFarCorners[0],
        _frustum.mNearCorners[0], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        fvec2(0.0f, 1.0f),
        _colorTop);
    submeshQuads.addQuad(
        _frustum.mNearCorners[3],
        _frustum.mFarCorners[3],
        _frustum.mFarCorners[2],
        _frustum.mNearCorners[2], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        fvec2(0.0f, 1.0f),
        _colorBottom);
    submeshQuads.addQuad(
        _frustum.mNearCorners[0],
        _frustum.mFarCorners[0],
        _frustum.mFarCorners[3],
        _frustum.mNearCorners[3], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        fvec2(0.0f, 1.0f),
        _colorLeft);
    submeshQuads.addQuad(
        _frustum.mNearCorners[2],
        _frustum.mFarCorners[2],
        _frustum.mFarCorners[1],
        _frustum.mNearCorners[1], //
        fvec2(0.0f, 0.0f),
        fvec2(1.0f, 0.0f),
        fvec2(1.0f, 1.0f),
        fvec2(0.0f, 1.0f),
        _colorRight);

    submeshTriangulate(submeshQuads, submeshTris);

    _primitive.fromSubMesh(submeshTris, context);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline void draw(Context* context) {
    OrkAssert(_primitive._vertexBuffer.get() != nullptr);
    _primitive.draw(context);
  }

  //////////////////////////////////////////////////////////////////////////////

  inline scenegraph::node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      materialinst_ptr_t material_inst) {
    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID, //
                               lev2::Context* context,
                               const CallbackRenderable* pren) { //
      material_inst->wrappedDrawCall(RCID, [this, context]() { this->draw(context); });
    });
    return layer->createNode(named, drw);
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
  meshutil::PrimitiveV12N12B12T8C4 _primitive;
};

using frustum_ptr_t = std::shared_ptr<FrustumPrimitive>;

} // namespace ork::lev2::primitives
