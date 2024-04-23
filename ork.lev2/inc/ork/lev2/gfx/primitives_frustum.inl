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

    auto addq      = [&](dvec3 vtxa, dvec3 vtxb, dvec3 vtxc, dvec3 vtxd, dvec4 col) {
      auto normal = (vtxb - vtxa).crossWith(vtxc - vtxa).normalized();

      auto uva = dvec2(0,0);
      auto uvb = dvec2(1,0);
      auto uvc = dvec2(1,1);
      auto uvd = dvec2(0,1);

      if(projective_rect_uv){
        
        // https://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
        
        dray3 rac(vtxa,(vtxc-vtxa));
        dvec3 intersection;
        bool intersects = rac.intersectSegment(dlineseg3(vtxb,vtxd),intersection);
        OrkAssert(intersects);

        float da = (intersection-vtxa).length();
        float db = (intersection-vtxb).length();
        float dc = (intersection-vtxc).length();
        float dd = (intersection-vtxd).length();

        auto uvqa = dvec3(uva.x,uva.y,1) * ((da+dc)/dc);
        auto uvqb = dvec3(uvb.x,uvb.y,1) * ((db+dd)/dd);
        auto uvqc = dvec3(uvc.x,uvc.y,1) * ((dc+da)/da);
        auto uvqd = dvec3(uvd.x,uvd.y,1) * ((dd+db)/db);

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
  inline scenegraph::drawable_node_ptr_t createNodeWithMaterial(
      std::string named, //
      scenegraph::layer_ptr_t layer,
      material_ptr_t material) {
    auto drw = std::make_shared<CallbackDrawable>(nullptr);

    //auto permu = std::make_shared<FxPipelinePermutation>();
    auto fxcache = material->pipelineCache();




    drw->SetRenderCallback([=](lev2::RenderContextInstData& RCID) { //
      auto context = RCID.context();
      auto material_inst = fxcache->findPipeline(RCID);
      material_inst->wrappedDrawCall(RCID, //
                                     [this, context]() { //
                                        this->renderEML(context); //
                                    });
    });
    auto node = layer->createDrawableNode(named, drw);
    //node->_userdata->template makeValueForKey<fxpipelinepermutation_ptr_t*>("_permu") = permu;
    node->_userdata->template makeValueForKey<fxpipelinecache_constptr_t>("_fxcache") = fxcache;
    //node->_userdata->template makeValueForKey<fxpipeline_ptr_t>("_mtlinst") = material_inst;
    return node;
  }
  //////////////////////////////////////////////////////////////////////////////
  dvec4 _colorTop;
  dvec4 _colorBottom;
  dvec4 _colorNear;
  dvec4 _colorFar;
  dvec4 _colorLeft;
  dvec4 _colorRight;
  ork::dfrustum _frustum;
  using rigidprim_t = meshutil::RigidPrimitive<SVtxV12N12B12T8C4>;
  rigidprim_t _primitive;
};
using frustum_ptr_t = std::shared_ptr<FrustumPrimitive>;
} // namespace ork::lev2::primitives
