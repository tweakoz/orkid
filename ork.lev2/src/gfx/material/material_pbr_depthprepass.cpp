////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
//
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbr_unl = logger()->configureChannel("mtlpbrDPP", fvec3(0.8, 0.8, 0.1), true);

fxpipeline_ptr_t PBRMaterial::_createFxPipelineDPP(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;

  // A3 — masked (alpha-tested) depth prepass. "active" cutoff is defined
  // exactly as the color pass discard gate (AlphaCutoff > 0): such materials
  // must not depth-occlude / shadow through their cutout holes. Materials
  // without an active cutoff keep the IDENTICAL technique objects below.
  const bool masked = (this->_alphaCutoff > 0.0f);
  auto l_masked_bind = [this](const RenderContextInstData& RCID) {
    // the forward lighting lambda NOPs under DEPTH_PREPASS, so the masked
    // depth fragment's albedo-alpha inputs are bound here instead — to the
    // DEDICATED DppCNMREA/DppAlphaCutoff resources: the masked fragment must
    // not touch shared ublk_std_pbr / sset_std_pbr (see pbrtools.i2 note).
    auto FXI = RCID.rcfd()->GetTarget()->FXI();
    OrkAssert(this->_texArrayCNMREA != nullptr); // masked DPP with no color array would sample garbage
    FXI->bindParamTextureArray(this->_paramDppCNMREA, this->_texArrayCNMREA.get());
    FXI->bindParamFloat(this->_paramDppAlphaCutoff, this->_alphaCutoff);
  };

  // SINGLE-PASS STEREO arms of the SSBO family, checked BEFORE their mono twins.
  //
  // "Same position math as the color pass -> no z-fight" is the whole contract of a depth prepass,
  // and under single-pass stereo the color pass is PER-VIEW (spvr_vp[view] out of ublk_stereo). A
  // mono prepass inside that pass writes ONE eye's depth into BOTH layers, so the eye whose
  // disparity runs against the mono camera fails LEQUALS across the entire surface: it renders
  // BLACK, and the sky behind it is rejected too (the prepass already wrote depth there). That is
  // the swest black-right-eye defect. The per-view techniques have been in the generated template
  // since the ST lowering; nothing selected them.
  //
  // No MVP bind on these arms (the clip transform is spvr_vp[view]), and the basic-state lambda is
  // what WRITES and binds ublk_stereo — same shape as the hand-authored FWD_DEPTHPREPASS_*_ST arm
  // below. Each is null-guarded: a material without the peer falls through to its mono twin
  // unchanged, so the mono path is byte-untouched.
  if (permu._stereo and permu._is_mesh_shader and this->_tek_FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS_ST) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS_ST;
    pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(false);
      mut->_rasterstate->setWriteMaskA(false);
    });
  }
  else if (permu._stereo and permu._is_vertex_ssbo and permu._instanced //
           and this->_tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS_ST) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS_ST;
    pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(false);
      mut->_rasterstate->setWriteMaskA(false);
    });
  }
  else if (permu._stereo and permu._is_vertex_ssbo and (not permu._instanced) //
           and this->_tek_FWD_SSBO_CUSTOM_DEPTHPREPASS_ST) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_DEPTHPREPASS_ST;
    pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(false);
      mut->_rasterstate->setWriteMaskA(false);
    });
  }
  // MESH-SHADER depth-prepass (FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS): the mesh twin of the SSBO-pull
  // depth pass — same meshlet decode and the same mvp*position as its color pass, so no z-fight.
  // Checked FIRST for the same reason the forward branch is: a mesh-sourced drawable also raises
  // _is_vertex_ssbo, and its draw call is DrawMeshTasksEML.
  else if (permu._is_mesh_shader and this->_tek_FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_MESH_DEPTHPREPASS;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(false);
      mut->_rasterstate->setWriteMaskA(false);
    });
  }
  // E.4 — INSTANCED SSBO depth-prepass: per-instance matrix placement, depth-only.
  // Checked BEFORE the non-instanced SSBO branch (which would draw every instance
  // at identity). Falls through when the material's shader lacks the variant.
  else if (permu._is_vertex_ssbo and permu._instanced and this->_tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_INSTANCED_DEPTHPREPASS;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(false);
      mut->_rasterstate->setWriteMaskA(false);
    });
  }
  // SSBO-sourced depth-prepass (FWD_SSBO_CUSTOM_DEPTHPREPASS): a first-class vertex variant, like
  // the FWD path. Same SSBO-pull `position` -> matching depth, no z-fight with the color pass.
  else if (permu._is_vertex_ssbo and (not permu._instanced) and this->_tek_FWD_SSBO_CUSTOM_DEPTHPREPASS) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_DEPTHPREPASS;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
    pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
      auto mut = const_cast<PBRMaterial*>(this);
      mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
      mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
      mut->_rasterstate->setWriteMaskZ(true);
      mut->_rasterstate->setWriteMaskRGB(false);
      mut->_rasterstate->setWriteMaskA(false);
    });
  }
  else if ((not permu._instanced) and (not permu._skinned)) {
    // SINGLE-PASS STEREO gets its own authored _ST technique (per-view clip transform);
    //  null-guarded, so a build without it falls through to the mono technique instead of
    //  producing a pipeline with a null technique.
    //  The masked peer is chosen the same way the mono branch below chooses its
    //  own: a cutout material whose stereo arm took the UNMASKED technique would
    //  write depth for texels the color pass discards.
    auto tek_st = (masked and this->_tek_FWD_DEPTHPREPASS_MASKED_RI_NI_ST) //
                      ? this->_tek_FWD_DEPTHPREPASS_MASKED_RI_NI_ST
                      : this->_tek_FWD_DEPTHPREPASS_RI_NI_ST;
    if (permu._stereo and tek_st) {
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = tek_st;
      pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
      if (tek_st == this->_tek_FWD_DEPTHPREPASS_MASKED_RI_NI_ST)
        pipeline->addStateLambda(l_masked_bind);
      pipeline->addStateLambda(createBasicStateLambda(this));
      pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
        auto mut = const_cast<PBRMaterial*>(this);
        mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
        mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
        mut->_rasterstate->setWriteMaskZ(true);
        mut->_rasterstate->setWriteMaskRGB(false);
        mut->_rasterstate->setWriteMaskA(false);
      });
    } else {

      auto tek = (masked and this->_tek_FWD_DEPTHPREPASS_MASKED_RI_NI_MO) //
                     ? this->_tek_FWD_DEPTHPREPASS_MASKED_RI_NI_MO
                     : this->_tek_FWD_DEPTHPREPASS_RI_NI_MO;
      if (tek) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = tek;
        pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
        pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh);
        if (tek == this->_tek_FWD_DEPTHPREPASS_MASKED_RI_NI_MO)
          pipeline->addStateLambda(l_masked_bind);
        //pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
          auto mut = const_cast<PBRMaterial*>(this);
          auto RCFD    = RCID.rcfd();
          auto context = RCFD->GetTarget();
          auto FXI     = context->FXI();
          auto MTXI    = context->MTXI();
          //auto RSI     = context->RSI();
          mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
          mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
          mut->_rasterstate->setWriteMaskZ(true);
          mut->_rasterstate->setWriteMaskRGB(false);
          mut->_rasterstate->setWriteMaskA(false);
          //RSI->BindRasterState(this->_rasterstate);
        });
      }
      else{
        logchan_pbr_unl->log( "mtl<%s> NO _tek_FWD_DEPTHPREPASS_RI_NI_MO\n", mMaterialName.c_str() );
      
      }
    }
  } else if (not permu._instanced and permu._skinned) {
    {
      // stereo first, masked-aware, each arm null-guarded so a missing peer
      //  falls through to the mono technique rather than to a null pipeline.
      auto tek = (masked and this->_tek_FWD_DEPTHPREPASS_MASKED_SK_NI_MO) //
                     ? this->_tek_FWD_DEPTHPREPASS_MASKED_SK_NI_MO
                     : this->_tek_FWD_DEPTHPREPASS_SK_NI_MO;
      if (permu._stereo) {
        auto tek_st = (masked and this->_tek_FWD_DEPTHPREPASS_MASKED_SK_NI_ST) //
                          ? this->_tek_FWD_DEPTHPREPASS_MASKED_SK_NI_ST
                          : this->_tek_FWD_DEPTHPREPASS_SK_NI_ST;
        if (tek_st)
          tek = tek_st;
      }
      if (tek) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = tek;
        pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
        if ((tek == this->_tek_FWD_DEPTHPREPASS_MASKED_SK_NI_MO) or
            (tek == this->_tek_FWD_DEPTHPREPASS_MASKED_SK_NI_ST))
          pipeline->addStateLambda(l_masked_bind);
        pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
          auto mut = const_cast<PBRMaterial*>(this);
          auto RCFD    = RCID.rcfd();
          auto context = RCFD->GetTarget();
          auto FXI     = context->FXI();
          auto MTXI    = context->MTXI();
          //auto RSI     = context->RSI();
          mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
          mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
          mut->_rasterstate->setWriteMaskZ(true);
          mut->_rasterstate->setWriteMaskRGB(false);
          mut->_rasterstate->setWriteMaskA(false);
          //RSI->BindRasterState(this->_rasterstate);
        });
      }
    }
  } else if (permu._instanced and not permu._skinned) {
    auto tek = (masked and this->_tek_FWD_DEPTHPREPASS_MASKED_RI_IN_MO) //
                   ? this->_tek_FWD_DEPTHPREPASS_MASKED_RI_IN_MO
                   : this->_tek_FWD_DEPTHPREPASS_RI_IN_MO;
    if (permu._stereo) {
      auto tek_st = (masked and this->_tek_FWD_DEPTHPREPASS_MASKED_RI_IN_ST) //
                        ? this->_tek_FWD_DEPTHPREPASS_MASKED_RI_IN_ST
                        : this->_tek_FWD_DEPTHPREPASS_RI_IN_ST;
      if (tek_st)
        tek = tek_st;
    }
    if (tek) {
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = tek;
      pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
      pipeline->bindParam(this->_paramDppZBias, "RCFD_PBR_DPP_ZBIAS"_crcsh); // the DPP fragment reads it
      if ((tek == this->_tek_FWD_DEPTHPREPASS_MASKED_RI_IN_MO) or
          (tek == this->_tek_FWD_DEPTHPREPASS_MASKED_RI_IN_ST))
        pipeline->addStateLambda(l_masked_bind);
      pipeline->addStateLambda(createBasicStateLambda(this));
      pipeline->addStateLambda([this](const RenderContextInstData& RCID) {
        auto mut = const_cast<PBRMaterial*>(this);
        auto RCFD    = RCID.rcfd();
        auto context = RCFD->GetTarget();
        auto FXI     = context->FXI();
        auto MTXI    = context->MTXI();
        //auto RSI     = context->RSI();
        mut->_rasterstate->setCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
        mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
        mut->_rasterstate->setWriteMaskZ(true);
        mut->_rasterstate->setWriteMaskRGB(false);
        mut->_rasterstate->setWriteMaskA(false);
        //RSI->BindRasterState(this->_rasterstate);
      });
    }
  }
  if(nullptr==pipeline){
    logchan_pbr_unl->log( "mtl<%s> NO DEPTH PREPASS\n", mMaterialName.c_str() );
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
  }
  return pipeline;
}

} // namespace ork::lev2
