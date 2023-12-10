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
#include <ork/lev2/gfx/gfxvtxbuf.inl>

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
    uint32_t boneID;
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
  fvec3 eye_dir = cammats->_vmatrix.inverse().zNormal().normalized();
  //printf("eye_pos<%g %g %g>\n", eye_pos.x, eye_pos.y, eye_pos.z);

  //////////////

  using vertex_t = SVtxV12N12T8DU12C4;
  auto vtxbuf   = context->miscVertexBuffer<vertex_t>("SKELETONS"_crcu, 256<<10);
  VtxWriter<vertex_t> vw;

  vertex_t hvtx, t;
  hvtx._uv       = fvec2(0, 0);
  hvtx._normal   = fvec3(0, 0, 1);
  t._uv          = fvec2(0, 0);
  t._normal      = fvec3(0, 0, 1);
  vw.Lock(context, vtxbuf.get(), inumjoints * 64);
  //vw.Lock(context, &vtxbuf, inumjoints * 64);

  std::multimap<float, Triangle> depth_sorted_triangles;

  auto enqueue_bones = [&](bool outline) {
    auto W_INVERSE = WorldMat.inverse();

    int inumbones = _skeleton->numBones();

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
      if ((ch_props->_numVerticesInfluenced == 0) && (pa_props->_numVerticesInfluenced == 0)) {
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

      auto BONECOLOR = fvec3(1, 0.5, 0);

      if( ch_props->_children.size() == 0){
        int ipp = _skeleton->jointParent(iparent);
        fmtx4 joint_pp   = localpose->_concat_matrices[ipp];
        fvec3 pp          = joint_pp.translation();
        bonelength = (pp - p).magnitude();
        auto chname = _skeleton->jointName(ichild);
        //printf("%s bonelength<%g>\n",chname.c_str(), bonelength);
        //if(bonelength>0.05){
          //bonelength = 0.05;
        //}
        BONECOLOR = fvec3(1, 0, 1);
      }
      if( ch_props->_children.size() > 1){
        BONECOLOR = fvec3(0.35, 0.75, 0.35 );
      }
      if(iparent==_skeleton->miRootNode){
        bonelength = 0.2;
        BONECOLOR = fvec3(1, 0, 0);
      }
      float bl2 = bonelength * 0.1f;
      auto add_triangle = [&](fvec3 posa,
                              fvec3 cola, //
                              fvec3 posb,
                              fvec3 colb, //
                              fvec3 posc,
                              fvec3 colc) { //
        fvec3 ctr   = (posa + posb + posc) * 0.33333333f;

        fvec3 wctr = fvec4(ctr).transform(joint_par).xyz();

        float depth = (wctr - eye_pos).magnitude();

        if(outline){ // depth bias outline
          depth += 0.1f*bonelength;          
        }
        else if(not is_pick){ 
          // color render : show wireframe
          //  by making triangles a bit smaller
          //  and leaking the outline layer through
          posa = ctr+(posa-ctr)*0.8;
          posb = ctr+(posb-ctr)*0.8;
          posc = ctr+(posc-ctr)*0.8;
        }
        auto tri = Triangle{
            //
            uint32_t(ib),
            joint_par, //
            posa,
            cola, //
            posb,
            colb, //
            posc,
            colc //
        };

        depth_sorted_triangles.insert(std::make_pair(depth, tri));
      };

      auto colorN = outline ? fvec3(0,0,.25) : BONECOLOR;
      auto colorX = outline ? fvec3(0,0,.25) : BONECOLOR;
      auto colorZ = outline ? fvec3(0,0,.25) : BONECOLOR;

      ///////////////////
      // create bone vertices (pyramid)
      ///////////////////

      dir      = fvec4(dir, 0).transform(joint_par.inverse()).xyz().normalized();
      auto ctr = fvec4(dir * bl2);

      auto chi_ctr = fvec3(0, bonelength * 0.5, 0);
      auto par_ctr = fvec3(0, 0, 0);

      auto px = par_ctr + fvec3(bl2, 0, 0);
      auto pz = par_ctr + fvec3(0, 0, bl2);
      auto nx = par_ctr + fvec3(-bl2, 0, 0);
      auto nz = par_ctr + fvec3(0, 0, -bl2);

      // add triangles for bone 
      // (maintaining counter-clockwise winding)

      par_ctr = fvec3(0,-bonelength*0.1,0);

      if(outline){

        add_triangle(par_ctr, colorN, nz, colorX, nx, colorZ);
        add_triangle(par_ctr, colorN, px, colorX, nz, colorZ);
        add_triangle(par_ctr, colorN, pz, colorX, px, colorZ);
        add_triangle(par_ctr, colorN, nx, colorX, pz, colorZ);

        // square to child

        add_triangle(chi_ctr, colorN, nx, colorX, nz, colorZ);
        add_triangle(chi_ctr, colorN, nz, colorX, px, colorZ);
        add_triangle(chi_ctr, colorN, px, colorX, pz, colorZ);
        add_triangle(chi_ctr, colorN, pz, colorX, nx, colorZ);

      }
      else{
        add_triangle(par_ctr, colorN, nx, colorX, nz, colorZ);
        add_triangle(par_ctr, colorN, nz, colorX, px, colorZ);
        add_triangle(par_ctr, colorN, px, colorX, pz, colorZ);
        add_triangle(par_ctr, colorN, pz, colorX, nx, colorZ);

        // square to child

        add_triangle(chi_ctr, colorN, nz, colorX, nx, colorZ);
        add_triangle(chi_ctr, colorN, px, colorX, nz, colorZ);
        add_triangle(chi_ctr, colorN, pz, colorX, px, colorZ);
        add_triangle(chi_ctr, colorN, nx, colorX, pz, colorZ);

      }


    } // for (int ib = 0; ib < inumbones; ib++) {

    /////////////////////////////////
    // add depth sorted triangles
    //. to vertex buffer
    /////////////////////////////////

    auto add_vertex = [&](const fmtx4& J, const fvec3 pos, const fvec4& col, const fvec3& N, uint32_t BID) {


      hvtx._position = fvec4(pos).transform(J).xyz();

      hvtx._color    = col.ABGRU32();
      if(_skeleton->_selboneindex>=0){
        if(BID==_skeleton->_selboneindex){
          hvtx._color    = 0xff00ffff;
        }
      }
      hvtx._normal   = N;
      hvtx._uv       = fvec2(0, 0);
      hvtx._data[0] = BID;
      hvtx._data[1] = 0;
      hvtx._data[2] = 0;

      vw.AddVertex(hvtx);
    };


    for (auto it_tri = depth_sorted_triangles.rbegin(); //
          it_tri != depth_sorted_triangles.rend();       //
          ++it_tri) {                                   //

      auto& tri      = it_tri->second;
      auto joint_par = tri._jnt;

      auto w_rot_mat4 = joint_par;
      auto w_rot_mat = w_rot_mat4.rotMatrix33();

      auto obj_normal = (tri._posB - tri._posA) //
                      . crossWith(tri._posC - tri._posA) //
                      .normalized();

      auto wld_normal = obj_normal.transform(w_rot_mat).normalized();
      float dot = wld_normal.dotWith(eye_dir);
      if(dot<0.0f){
        dot = 0.0f;
      }

      add_vertex(joint_par, tri._posA, tri._colA*dot, wld_normal, tri.boneID);
      add_vertex(joint_par, tri._posB, tri._colB*dot, wld_normal, tri.boneID);
      add_vertex(joint_par, tri._posC, tri._colC*dot, wld_normal, tri.boneID);
    }

  }; // auto enqueue_bones = [&](bool outline){

  if (not is_pick) {
    enqueue_bones(true); // outlines
  }
  enqueue_bones(false); // interiors

  ///////////////////
  // render bones
  ///////////////////

  // todo - per-bone selection 
  //  use pickbuffer heirarchical partition method
  //
  // 8 bits - pickbuffer method encoding
  // 
  // method 0 (object only)
  // 8  bits (value 0) : Method 0
  // 56 bits : object id
  // 
  // method 1 (object/component/subcomponent)
  //  8 bits : Method 1
  // 16 bits : object id
  //  8 bits : component-id
  // 32 bits : sub-component-id

  vw.UnLock(context);
  context->MTXI()->PushMMatrix(fmtx4::Identity());
  //RCIDCOPY._pickID = fvec4(1, 0, 0, 1);
  RCIDCOPY._pickID = fvec4(1, 1, 0, 1);
  use_mtl->_rasterstate.SetDepthTest(EDepthTest::OFF);
  use_mtl->_rasterstate.SetCullTest(ECullTest::PASS_FRONT);
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
