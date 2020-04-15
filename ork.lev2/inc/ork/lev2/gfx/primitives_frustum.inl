#pragma once
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/math/frustum.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>

namespace ork::lev2::primitives {
//////////////////////////////////////////////////////////////////////////////
struct FrustumPrimitive {
  //////////////////////////////////////////////////////////////////////////////
  inline void gpuInit(Context* context) {
    meshutil::submesh frustum_submesh;
    frustum_submesh.addQuad(
        _frustum.mNearCorners[3],
        _frustum.mNearCorners[2],
        _frustum.mNearCorners[1],
        _frustum.mNearCorners[0], //
        _colorFront);
    frustum_submesh.addQuad(
        _frustum.mFarCorners[0],
        _frustum.mFarCorners[1],
        _frustum.mFarCorners[2],
        _frustum.mFarCorners[3], //
        _colorBack);
    frustum_submesh.addQuad(
        _frustum.mNearCorners[1],
        _frustum.mFarCorners[1],
        _frustum.mFarCorners[0],
        _frustum.mNearCorners[0], //
        _colorTop);
    frustum_submesh.addQuad(
        _frustum.mNearCorners[3],
        _frustum.mFarCorners[3],
        _frustum.mFarCorners[2],
        _frustum.mNearCorners[2], //
        _colorBottom);
    frustum_submesh.addQuad(
        _frustum.mNearCorners[0],
        _frustum.mFarCorners[0],
        _frustum.mFarCorners[3],
        _frustum.mNearCorners[3], //
        _colorLeft);
    frustum_submesh.addQuad(
        _frustum.mNearCorners[2],
        _frustum.mFarCorners[2],
        _frustum.mFarCorners[1],
        _frustum.mNearCorners[1], //
        _colorRight);
    _primitive.fromSubMesh(frustum_submesh, context);
  }
  //////////////////////////////////////////////////////////////////////////////
  inline void renderEML(Context* context) {
    _primitive.renderEML(context);
  }
  //////////////////////////////////////////////////////////////////////////////
  inline scenegraph::node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      materialinst_ptr_t material_inst) {
    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      material_inst->wrappedDrawCall(RCID, [this, context]() { this->renderEML(context); });
    });
    return layer->createNode(named, drw);
  }
  //////////////////////////////////////////////////////////////////////////////
  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorFront;
  fvec4 _colorBack;
  fvec4 _colorLeft;
  fvec4 _colorRight;
  ork::Frustum _frustum;
  using rigidprim_t = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  rigidprim_t _primitive;
};
using frustum_ptr_t = std::shared_ptr<FrustumPrimitive>;
} // namespace ork::lev2::primitives
