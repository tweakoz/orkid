////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/environment.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>

template class ork::orklut<ork::PoolString, ork::lev2::xgmmesh_ptr_t>;
namespace ork::lev2 {

void XgmModel::RenderSkeleton(
    const XgmModelInst* minst,
    const fcolor4& ModColor,
    const fmtx4& WorldMat,
    ork::lev2::Context* context,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {

  auto RCFD        = context->topRenderContextFrameData();
  const auto& CPD  = RCFD->topCPD();


  ////////////////////////////////////////
  // Draw Skeleton

  auto world_mtx = context->MTXI()->RefMMatrix();

  const auto& worldpose = minst->_worldPose;
  const auto& localpose = minst->_localPose;

  context->debugPushGroup("DrawSkeleton");
  context->PushModColor(fvec4::White());

  static pbrmaterial_ptr_t material_sk = std::make_shared<PBRMaterial>(context);
  static pbrmaterial_ptr_t material_pick = std::make_shared<PBRMaterial>(context);
  material_sk->mMaterialName = AddPooledString("mtl-sk");
  material_sk->_variant = "vertexcolor"_crcu;
  material_pick->mMaterialName = AddPooledString("mtl-sk-pick");
  material_pick->_variant = "vertexcolor"_crcu;;

  bool is_pick = CPD.isPicking();

  if( RCFD->hasUserProperty("is_sg_pick"_crc) ){
    //OrkBreak();
  }

  auto use_mtl = is_pick //
               ? material_pick   //
               : material_sk;
    //OrkBreak();


  use_mtl->_rasterstate.SetDepthTest(EDepthTest::OFF);
  use_mtl->_rasterstate.SetZWriteMask(true);
  auto fxcache = use_mtl->pipelineCache();
  OrkAssert(fxcache);
  RenderContextInstData RCIDCOPY = RCID;
  RCIDCOPY._isSkinned            = false;
  RCIDCOPY._pipeline_cache       = fxcache;

  auto pipeline                  = fxcache->findPipeline(RCIDCOPY);
  OrkAssert(pipeline);

  if( RCFD->hasUserProperty("is_sg_pick"_crc) ){
    printf("pipeline tek<%s> is_pick<%d> CPD<%s>\n", pipeline->_technique->mTechniqueName.c_str(), int(is_pick), CPD._debugName.c_str() );
  }
        
  //////////////

  {
    using vertex_t = SVtxV12N12B12T8C4;
    auto& vtxbuf = GfxEnv::GetSharedDynamicVB2();
    VtxWriter<vertex_t> vw;
    int inumjoints = _skeleton->miNumJoints;
    // printf("inumjoints<%d>\n", inumjoints);
    if (inumjoints) {
      vertex_t hvtx, t;
      hvtx.mColor    = uint32_t(0xff00ffff);
      t.mColor       = uint32_t(0xff0000ff);
      hvtx.mUV0      = fvec2(0, 0);
      t.mUV0         = fvec2(1, 1);
      hvtx.mNormal   = fvec3(0, 0, 0);
      t.mNormal      = fvec3(1, 0, 0);
      hvtx.mBiNormal = fvec3(1, 1, 0);
      t.mBiNormal    = fvec3(1, 1, 0);
      vw.Lock(context, &vtxbuf, inumjoints * 32);

      auto W_INVERSE = WorldMat.inverse();

      int inumbones = _skeleton->numBones();
      for (int ib = 0; ib < inumbones; ib++) {

        const XgmBone& bone = _skeleton->bone(ib);
        int iparent         = bone._parentIndex;
        int ichild          = bone._childIndex;

        if (iparent < 0) {
          continue;
        }

        auto ch_props = _skeleton->_jointProperties[ichild];
        auto pa_props = _skeleton->_jointProperties[iparent];
        OrkAssert(ch_props != nullptr);
        if ((ch_props->_numVerticesInfluenced == 0) || (pa_props->_numVerticesInfluenced == 0)) {
          continue;
        }
        // if(pa_props->_numVerticesInfluenced == 0){
        // continue;
        //}
        auto sk_par = _skeleton->_jointMatrices[iparent].translation();
        auto sk_chi = _skeleton->_jointMatrices[ichild].translation();
        // float bonelength = (sk_chi-sk_par).magnitude();

        fmtx4 joint_par   = localpose->_concat_matrices[iparent];
        fmtx4 joint_child = localpose->_concat_matrices[ichild];

        fvec3 c          = joint_child.translation();
        fvec3 p          = joint_par.translation();
        fvec3 dir        = (c - p).normalized();
        float bonelength = (c - p).magnitude();
        float bl2        = bonelength * 0.1f;

        auto add_vertex = [&](const fvec3 pos, const fvec3& col) {
          hvtx.mPosition = fvec4(pos).transform(joint_par).xyz();
          hvtx.mColor    = (col * 5).ABGRU32();
          vw.AddVertex(hvtx);
        };

        // create bone vertices (pyramid)
        dir      = fvec4(dir, 0).transform(joint_par.inverse()).xyz();
        auto ctr = fvec4(dir * bl2);


        auto colorN = fvec3::White();
        auto colorX = fvec3(1, .7, .7);
        auto colorZ = fvec3(.7, .7, 1);

        auto chi_ctr = fvec3(0, bonelength * 0.5, 0);
        auto par_ctr = fvec3(0, 0, 0);

        auto px  = par_ctr+fvec4(bl2, 0, 0);
        auto pz  = par_ctr+fvec4(0, 0, bl2);
        auto nx  = par_ctr+fvec4(-bl2, 0, 0);
        auto nz  = par_ctr+fvec4(0, 0, -bl2);

        // add triangles for bone (maintaining counter-clockwise winding)

        add_vertex(par_ctr, colorN);
        add_vertex(nx, colorN);
        add_vertex(nz, colorN);

        add_vertex(par_ctr, colorN);
        add_vertex(nx, colorN);
        add_vertex(pz, colorN);

        add_vertex(par_ctr, colorN);
        add_vertex(px, colorN);
        add_vertex(pz, colorN);

        add_vertex(par_ctr, colorN);
        add_vertex(px, colorN);
        add_vertex(nz, colorN);

        add_vertex(par_ctr, colorN);
        add_vertex(nx, colorN);
        add_vertex(pz, colorN);
     
        // square to child

        add_vertex(chi_ctr, colorN);
        add_vertex(nx, colorN);
        add_vertex(nz, colorN);

        add_vertex(chi_ctr, colorN);
        add_vertex(nx, colorN);
        add_vertex(pz, colorN);

        add_vertex(chi_ctr, colorN);
        add_vertex(px, colorN);
        add_vertex(pz, colorN);

        add_vertex(chi_ctr, colorN);
        add_vertex(px, colorN);
        add_vertex(nz, colorN);


      }
      vw.UnLock(context);
      context->MTXI()->PushMMatrix(fmtx4::Identity());
      RCIDCOPY._pickID = fvec4(1,0,1,1);
      pipeline->wrappedDrawCall(RCIDCOPY, [&]() { //
        context->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES); 
      });
      context->MTXI()->PopMMatrix();
    }
  }
  context->PopModColor();
  context->debugPopGroup();
}

} // namespace ork::lev2
