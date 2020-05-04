#pragma once
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/math/frustum.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/material_instance.h>

namespace ork::lev2::primitives {
//////////////////////////////////////////////////////////////////////////////
struct FrustumPrimitive {
  //////////////////////////////////////////////////////////////////////////////
  inline void gpuInit(Context* context) {
    meshutil::submesh frustum_submesh;
    const auto& NC = _frustum.mNearCorners;
    const auto& FC = _frustum.mFarCorners;
    auto addq      = [&](fvec3 vtxa, fvec3 vtxb, fvec3 vtxc, fvec3 vtxd, fvec4 col) {
      auto normal = (vtxb - vtxa).Cross(vtxc - vtxa).Normal();
      frustum_submesh.addQuad(
          vtxa, vtxb, vtxc, vtxd, normal, normal, normal, normal, fvec2(0, 0), fvec2(0, 1), fvec2(1, 1), fvec2(1, 0), col);
    };
    addq(NC[3], NC[2], NC[1], NC[0], _colorFront);
    addq(FC[0], FC[1], FC[2], FC[3], _colorBack);
    addq(NC[1], FC[1], FC[0], NC[0], _colorTop);
    addq(NC[3], FC[3], FC[2], NC[2], _colorBottom);
    addq(NC[0], FC[0], FC[3], NC[3], _colorLeft);
    addq(NC[2], FC[2], FC[1], NC[1], _colorRight);
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
      fxinstance_ptr_t material_inst) {
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
