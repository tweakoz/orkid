////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/math/frustum.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/lev2/gfx/fx_pipeline.h>

namespace ork::lev2::primitives {
//////////////////////////////////////////////////////////////////////////////
struct FrustumPrimitive {
  //////////////////////////////////////////////////////////////////////////////
  inline void gpuInit(Context* context, bool projective_rect_uv=true) {
    meshutil::submesh frustum_submesh;

    auto addq      = [&](fvec3 vtxa, fvec3 vtxb, fvec3 vtxc, fvec3 vtxd, fvec4 col) {
      auto normal = (vtxb - vtxa).crossWith(vtxc - vtxa).normalized();

      auto uva = fvec2(0,0);
      auto uvb = fvec2(1,0);
      auto uvc = fvec2(1,1);
      auto uvd = fvec2(0,1);

      if(projective_rect_uv){
        
        // https://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
        
        fray3 rac(vtxa,(vtxc-vtxa));
        fvec3 intersection;
        bool intersects = rac.intersectSegment(flineseg3(vtxb,vtxd),intersection);
        OrkAssert(intersects);

        float da = (intersection-vtxa).length();
        float db = (intersection-vtxb).length();
        float dc = (intersection-vtxc).length();
        float dd = (intersection-vtxd).length();

        auto uvqa = fvec3(uva.x,uva.y,1) * ((da+dc)/dc);
        auto uvqb = fvec3(uvb.x,uvb.y,1) * ((db+dd)/dd);
        auto uvqc = fvec3(uvc.x,uvc.y,1) * ((dc+da)/da);
        auto uvqd = fvec3(uvd.x,uvd.y,1) * ((dd+db)/db);

        frustum_submesh.addQuad(
          vtxa, vtxb, vtxc, vtxd, 
          normal, normal, normal, normal, 
          uvqa, uvqb, uvqc, uvqd, 
          uva, uvb, uvc, uvd, col);

      }
      else{
        frustum_submesh.addQuad(
          vtxa, vtxb, vtxc, vtxd, 
          normal, normal, normal, normal, 
          uva, uvb, uvc, uvd, col);
      }

    };

    const auto& NC = _frustum.mNearCorners;
    const auto& FC = _frustum.mFarCorners;
    auto NTL = NC[0];
    auto FTL = FC[0];
    auto NTR = NC[1];
    auto FTR = FC[1];
    auto NBR = NC[2];
    auto FBR = FC[2];
    auto NBL = NC[3];
    auto FBL = FC[3];

    addq(NBL, NBR, NTR, NTL, _colorNear);
    addq(FBL, FTL, FTR, FBR, _colorFar);

    addq(NTL, NTR, FTR, FTL, _colorTop);
    addq(NBR, NBL, FBL, FBR, _colorBottom);

    addq(NTL, FTL, FBL, NBL, _colorLeft);
    addq(NBR, FBR, FTR, NTR, _colorRight);

    _primitive.fromSubMesh(frustum_submesh, context);
  }
  //////////////////////////////////////////////////////////////////////////////
  inline void renderEML(Context* context) {
    _primitive.renderEML(context);
  }
  //////////////////////////////////////////////////////////////////////////////
  inline scenegraph::drawable_node_ptr_t createNode(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      fxpipeline_ptr_t material_inst) {
    auto drw = std::make_shared<CallbackDrawable>(nullptr);
    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      material_inst->wrappedDrawCall(RCID, //
                                     [this, context]() { //
                                        this->renderEML(context); //
                                    });
    });
    return layer->createDrawableNode(named, drw);
  }
  //////////////////////////////////////////////////////////////////////////////
  fvec4 _colorTop;
  fvec4 _colorBottom;
  fvec4 _colorNear;
  fvec4 _colorFar;
  fvec4 _colorLeft;
  fvec4 _colorRight;
  ork::Frustum _frustum;
  using rigidprim_t = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  rigidprim_t _primitive;
};
using frustum_ptr_t = std::shared_ptr<FrustumPrimitive>;
} // namespace ork::lev2::primitives
