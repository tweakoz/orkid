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
    const fmtx4& WorldMat,
    ork::lev2::Context* context,
    const RenderContextInstData& RCID) const {

  int inumjoints = _skeleton->miNumJoints;
  if (inumjoints == 0) {
    return;
  }

  auto RCFD       = context->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();

  ////////////////////////////////////////
  // Draw Skeleton

  auto world_mtx = context->MTXI()->RefMMatrix();

  const auto& worldpose = minst->_worldPose;
  const auto& localpose = minst->_localPose;

  context->debugPushGroup("DrawSkeleton");
  context->PushModColor(fvec4::White());

  bool is_pick = CPD.isPicking();

  ////////////////////////////
  // material selection
  // pickmode: pbr.glfx::PIK_RI_NI (vs_pick_rigid_mono,ps_pick)
  // defpbr: pbr.glfx::GBU_CV_EMI_RI_NI_MO (vs_rigid_gbuffer_vtxcolor,ps_gbuffer_vtxcolor)
  // defpbr-outline: pbr.glfx::GBU_CV_EMI_RI_NI_MO (vs_rigid_gbuffer_vtxcolor,ps_gbuffer_vtxcolor)
  // fwdpbr: pbr.glfx::??
  ////////////////////////////

  static pbrmaterial_ptr_t material_sk      = std::make_shared<PBRMaterial>(context);
  static pbrmaterial_ptr_t material_pick    = std::make_shared<PBRMaterial>(context);
  static pbrmaterial_ptr_t material_outline = std::make_shared<PBRMaterial>(context);
  material_outline->mMaterialName           = AddPooledString("mtl-outline");
  material_outline->_variant                = "vertexcolor"_crcu;
  material_sk->mMaterialName                = AddPooledString("mtl-sk");
  material_sk->_variant                     = "vertexcolor"_crcu;
  material_pick->mMaterialName              = AddPooledString("mtl-sk-pick");
  material_pick->_variant                   = 0;

  ////////////////////////////

  auto use_mtl = is_pick             //
                     ? material_pick //
                     : material_sk;

  //////////////
  // pipeline selection
  //////////////

  auto fxcache = use_mtl->pipelineCache();
  OrkAssert(fxcache);
  RenderContextInstData RCIDCOPY = RCID;
  RCIDCOPY._isSkinned            = false;
  RCIDCOPY._pipeline_cache       = fxcache;

  auto pipeline = fxcache->findPipeline(RCIDCOPY);
  OrkAssert(pipeline);

  struct Triangle {
    fmtx4 _jnt;
    fvec3 _posA;
    fvec3 _colA;
    fvec3 _posB;
    fvec3 _colB;
    fvec3 _posC;
    fvec3 _colC;
  };

  auto cammats  = CPD.cameraMatrices();
  fvec3 eye_pos = cammats->_vmatrix.inverse().translation();

  //////////////

  using vertex_t = SVtxV12N12B12T8C4;
  auto& vtxbuf   = GfxEnv::GetSharedDynamicVB2();
  VtxWriter<vertex_t> vw;

  vertex_t hvtx, t;
  hvtx.mColor    = uint32_t(0xff0000ff);
  t.mColor       = uint32_t(0xff0000ff);
  hvtx.mUV0      = fvec2(0, 0);
  t.mUV0         = fvec2(1, 1);
  hvtx.mNormal   = fvec3(0, 0, 0);
  t.mNormal      = fvec3(1, 0, 0);
  hvtx.mBiNormal = fvec3(1, 1, 0);
  t.mBiNormal    = fvec3(1, 1, 0);
  vw.Lock(context, &vtxbuf, inumjoints * 64);

  auto enqueue_bones = [&](bool outline) {
    auto W_INVERSE = WorldMat.inverse();

    int inumbones = _skeleton->numBones();
    std::multimap<float, Triangle> depth_sorted_triangles;
    std::vector<Triangle> unsorted_triangles;

    for (int ib = 0; ib < inumbones; ib++) {

      const XgmBone& bone = _skeleton->bone(ib);
      int iparent         = bone._parentIndex;
      int ichild          = bone._childIndex;

      ///////////////////
      // skip root bone
      ///////////////////

      if (iparent < 0) {
        continue;
      }

      ///////////////////
      // skip bones which have no influence on
      // any vertices
      ///////////////////

      auto ch_props = _skeleton->_jointProperties[ichild];
      auto pa_props = _skeleton->_jointProperties[iparent];
      OrkAssert(ch_props != nullptr);
      if ((ch_props->_numVerticesInfluenced == 0) || (pa_props->_numVerticesInfluenced == 0)) {
        continue;
      }

      ///////////////////

      auto sk_par = _skeleton->_jointMatrices[iparent].translation();
      auto sk_chi = _skeleton->_jointMatrices[ichild].translation();

      fmtx4 joint_par   = localpose->_concat_matrices[iparent];
      fmtx4 joint_child = localpose->_concat_matrices[ichild];

      fvec3 c          = joint_child.translation();
      fvec3 p          = joint_par.translation();
      fvec3 dir        = (c - p).normalized();
      float bonelength = (c - p).magnitude();

      if (outline) {
        bonelength *= 1.3f;
      }
      float bl2 = bonelength * 0.1f;

      auto add_triangle = [&](const fvec3 posa,
                              const fvec3& cola, //
                              const fvec3 posb,
                              const fvec3& colb, //
                              const fvec3 posc,
                              const fvec3& colc) { //
        fvec3 ctr   = (posa + posb + posc) * 0.33333333f;
        float depth = (ctr - eye_pos).magnitude();
        if(outline){
          depth += 0.001f;          
        }
        auto tri = Triangle{
            //
            joint_par, //
            posa,
            cola, //
            posb,
            colb, //
            posc,
            colc //
        };

        depth_sorted_triangles.insert(std::make_pair(depth, tri));
        unsorted_triangles.push_back(tri);
      };

      auto colorN = outline ? fvec3::Black() : fvec3(1, 0.5, 0);
      auto colorX = outline ? fvec3::Black() : fvec3(1, 0.5, 0);
      auto colorZ = outline ? fvec3::Black() : fvec3(1, 0.5, 0);

      ///////////////////
      // create bone vertices (pyramid)
      ///////////////////

      dir      = fvec4(dir, 0).transform(joint_par.inverse()).xyz();
      auto ctr = fvec4(dir * bl2);

      auto chi_ctr = fvec3(0, bonelength * 0.5, 0);
      auto par_ctr = fvec3(0, 0, 0);

      auto px = par_ctr + fvec3(bl2, 0, 0);
      auto pz = par_ctr + fvec3(0, 0, bl2);
      auto nx = par_ctr + fvec3(-bl2, 0, 0);
      auto nz = par_ctr + fvec3(0, 0, -bl2);


      // add triangles for bone (maintaining counter-clockwise winding)

      par_ctr = fvec3(0,-bonelength*0.1,0);

      add_triangle(par_ctr, colorN, nx, colorX, nz, colorZ);
      add_triangle(par_ctr, colorN, nx, colorX, pz, colorZ);
      add_triangle(par_ctr, colorN, px, colorX, pz, colorZ);
      add_triangle(par_ctr, colorN, px, colorX, nz, colorZ);

      // square to child

      add_triangle(chi_ctr, colorN, nx, colorX, nz, colorZ);
      add_triangle(chi_ctr, colorN, nx, colorX, pz, colorZ);
      add_triangle(chi_ctr, colorN, px, colorX, pz, colorZ);
      add_triangle(chi_ctr, colorN, px, colorX, nz, colorZ);

    } // for (int ib = 0; ib < inumbones; ib++) {

    /////////////////////////////////
    // add depth sorted triangles
    //. to vertex buffer
    /////////////////////////////////

    auto add_vertex = [&](const fmtx4& J, const fvec3 pos, const fvec3& col, const fvec3& N) {
      hvtx.mPosition = fvec4(pos).transform(J).xyz();
      hvtx.mColor    = (col * 5).ABGRU32();
      hvtx.mNormal   = N;
      vw.AddVertex(hvtx);
    };

    auto w_rot_mat = WorldMat;
    w_rot_mat.setColumn(3,fvec4(0, 0, 0,1));
    auto iw_rot_mat = w_rot_mat.inverse().rotMatrix33();
    for (auto it_tri = depth_sorted_triangles.rbegin(); //
          it_tri != depth_sorted_triangles.rend();       //
          ++it_tri) {                                   //
      auto& tri      = it_tri->second;
      auto joint_par = tri._jnt;
      auto wld_normal = (tri._posB - tri._posA).crossWith(tri._posC - tri._posA).normalized();
      auto obj_normal = wld_normal.transform(iw_rot_mat);
      //obj_normal = fvec3(obj_normal.x, obj_normal.z, obj_normal.y);
      //obj_normal = fvec3(0.5,0.5,0.5)+(obj_normal*(-0.5));
      add_vertex(joint_par, tri._posA, tri._colA, obj_normal);
      add_vertex(joint_par, tri._posB, tri._colB, obj_normal);
      add_vertex(joint_par, tri._posC, tri._colC, obj_normal);
    }

  }; // auto enqueue_bones = [&](bool outline){

  if (not is_pick) {
    enqueue_bones(true); // outlines
  }
  enqueue_bones(false); // interiors

  ///////////////////
  // render bones
  ///////////////////

  vw.UnLock(context);
  context->MTXI()->PushMMatrix(fmtx4::Identity());
  RCIDCOPY._pickID = fvec4(1, 0, 0, 1);
  use_mtl->_rasterstate.SetDepthTest(EDepthTest::OFF);
  use_mtl->_rasterstate.SetCullTest(ECullTest::OFF);
  use_mtl->_rasterstate.SetZWriteMask(false);
  pipeline->wrappedDrawCall(RCIDCOPY, [&]() { //
    context->RSI()->BindRasterState(use_mtl->_rasterstate);
    context->GBI()->DrawPrimitiveEML(vw, PrimitiveType::TRIANGLES);
  });
  context->MTXI()->PopMMatrix();
  ///////////////////////////
  context->PopModColor();
  context->debugPopGroup();
}

} // namespace ork::lev2
