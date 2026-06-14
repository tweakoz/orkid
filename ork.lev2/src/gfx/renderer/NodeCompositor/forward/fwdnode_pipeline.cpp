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

static logchannel_ptr_t logchan_pbr_fwd = logger()->configureChannel("mtlpbrFWDPL", fvec3(0.8, 0.8, 0.1), true);

FxPipeline::statelambda_t createForwardLightingLambda(const PBRMaterial* mtl) {

  auto L = [mtl](const RenderContextInstData& RCID) {

    auto RCFD             = RCID.rcfd();
    auto context    = RCFD->GetTarget();
    auto FXI        = context->FXI();
    bool is_skinned = RCID._isSkinned;
    auto CIMPL = RCFD->topCompositor();
    ///////////////////////////////////////////////////////////////////////////
    // retrieve global RCFD state for PBR materials
    ///////////////////////////////////////////////////////////////////////////

    auto enumlights = RCFD->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    bool is_rendering_PROBE = RCFD->userPropertyAs<bool>("renderingPROBE"_crcu);
    bool have_PROBES = RCFD->userPropertyAs<bool>("havePROBES"_crcu);
    bool should_bind_probes = (have_PROBES);
    bool is_depth_prepass = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;
    auto pbrcommon = RCFD->userPropertyAs<pbr::commonstuff_ptr_t>("PBR_COMMON"_crcu);

    ///////////////////////////////////////////////////////////////////////////
    // we are not lighting for depth prepass, so NOP
    ///////////////////////////////////////////////////////////////////////////

    if (is_depth_prepass)
      return;

    ///////////////////////////////////////////////////////////////////////////
    // build lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    // logchan_pbr_fwd->log("fwd: all lights count<%zu>", enumlights->_alllights.size());

    auto lmgr = CIMPL->_lightmgr;
    OrkAssert(lmgr);
    OrkAssert(lmgr->_needs_gpu_init==false);

    ///////////////////////////////////////////////////////////////////////////
    // bind lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    if (mtl->_parUnTexPointLightsCount) {
      FXI->bindParamInt(mtl->_parUnTexPointLightsCount, enumlights->_num_active_untextured_pointlights);
    }
    if (mtl->_parForwardLightBlock) {
      auto pl_buffer = PBRMaterial::lightingDataBuffer(context);
      if(0)printf("BINDING LIGHTING SSBO<%p> to param<%p> mtl<%s>\n", //
             (void*) pl_buffer,                          //
             mtl->_parForwardLightBlock,
             mtl->mMaterialName.c_str());


      FXI->bindStorageBuffer(mtl->_parForwardLightBlock, pl_buffer);
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind spotlight cookies
    ///////////////////////////////////////////////////////////////////////////
 
    if (mtl->_parTexSpotLightsCount) {
      FXI->bindParamInt(mtl->_parTexSpotLightsCount, enumlights->_num_active_texspotlights);
    }
    else {
      FXI->bindParamInt(mtl->_parTexSpotLightsCount, 0 );
    }
    FXI->bindParamTextureArray(mtl->_parLightDepthCookies, lmgr->_cookies_spot_depth.get() );
    FXI->bindParamTextureArray(mtl->_parLightColorCookies, lmgr->_cookies_spot_color.get() );

    ///////////////////////////////////////////////////////////////////////////
    // bind Color/Normal/Metallic/Roughness/AO Texture Array
    ///////////////////////////////////////////////////////////////////////////

    FXI->bindParamTextureArray( mtl->_paramMapCNMREA, mtl->_texArrayCNMREA.get() );

    ///////////////////////////////////////////////////////////////////////////
    // bind light/environment probes
    ///////////////////////////////////////////////////////////////////////////

    //printf("should_bind_probes<%d> is_rendering_PROBE<%d>\n", int(should_bind_probes), int(is_rendering_PROBE));
    bool probe_active = false;
    if(not is_rendering_PROBE){
      // Per-draw probe override (PBR2 P0.4c — set by ParticlesGlobalSystem
      // from gendata._probe_entity_name → live LightProbe). Routes a
      // specific probe's cube to this draw, regardless of the global
      // _lightprobes[0] choice. Empty → fall through to global path.
      lightprobe_ptr_t chosen_probe;
      if (RCID._probeOverride) {
        chosen_probe = RCID._probeOverride;
      } else if (should_bind_probes && enumlights->_lightprobes.size() > 0) {
        // Global fallback: first probe wins (same as the old behavior).
        chosen_probe = enumlights->_lightprobes[0];
      }
      if (chosen_probe) {
        auto probe_tex = chosen_probe->_cubeTexture;
        FXI->bindParamTexture(mtl->_parProbeReflection, probe_tex.get());
        FXI->bindParamTexture(mtl->_parProbeRadiance,   probe_tex.get());
        probe_active = (probe_tex != nullptr);
      }
    }
    if(not probe_active){
      //printf( "NOT BINDING PROBES black<%p>!\n", mtl->_texCubeBlack.get() );
      FXI->bindParamTexture(mtl->_parProbeReflection, pbrcommon->_texCubeBlack.get() );
      FXI->bindParamTexture(mtl->_parProbeRadiance, pbrcommon->_texCubeBlack.get() );
    }
    // has_reflection_probe drives the spec-env vs probe swap in fwdtools.i2.
    // Inactive path keeps the black cube bound so the sampler is always valid;
    // shader branches on the flag, not on sampler-null.
    if(mtl->_parHasReflectionProbe){
      FXI->bindParamInt(mtl->_parHasReflectionProbe, probe_active ? 1 : 0);
    }
    // PBR2 Phase 2 — rendering_probe gates refractive lobes during cubemap
    // capture. is_rendering_PROBE comes off RCFD["renderingPROBE"], set on
    // the per-face CPD in fwdnode_impl_sub.cpp before each probe pass.
    if(mtl->_parRenderingProbe){
      FXI->bindParamInt(mtl->_parRenderingProbe, is_rendering_PROBE ? 1 : 0);
    }

    ///////////////////////////////////////////////////////////////////////////
    // lightmaps
    ///////////////////////////////////////////////////////////////////////////

    /*for( int i=0; i<8; i++ ){
      auto C = mtl->_lightmapColors[i];
      printf( "lightmapcolor<%d> = <%f %f %f>\n", i, C.x, C.y, C.z );
    }*/
    if(mtl->_texLightMapArray){
      //printf("binding lightmap array\n");
      FXI->bindParamTextureArray(mtl->_parMapLightMapArray, mtl->_texLightMapArray.get());
      FXI->bindParamVect3Array(mtl->_paramLightMapColors, mtl->_lightmapColors,8);
    }
    else{
      //printf("binding white lightmap array\n");
      //printf("mtl->_parMapLightMapArray<%p>\n", mtl->_parMapLightMapArray);
      //printf("mtl->_texWhiteLightMapArray<%p>\n", mtl->_texWhiteLightMapArray.get());
      FXI->bindParamTextureArray(mtl->_parMapLightMapArray, pbrcommon->_texWhiteLightMapArray.get());
      FXI->bindParamVect3Array(mtl->_paramLightMapColors, mtl->_lightmapColors,8);
      //printf("OK...\n");
    }

    ///////////////////////////////////////////////////////////////////////////

    // ModColor: per-frame post-multiplicative tint (fades, hover, etc.).
    // Applied AFTER lighting computation. Must NOT carry albedo or it
    // would multiply through the env-IBL specular path.
    //
    // ModAlbedo: per-material multiplicative albedo tint. Applied at the
    // light-input boundary (modulates the textured CNMREA color sample).
    // Used by lighting math (diffuse term, F0 metallic mix), not as a
    // post-multiplier.
    auto modcolor = context->RefModColor();
    FXI->bindParamVect4(mtl->_parModColor,  modcolor);
    FXI->bindParamVect4(mtl->_parModAlbedo, mtl->_baseColor);
  };
  return L;
}

void PBRMaterial::addLightingLambda(fxpipeline_ptr_t pipe) {
  auto L = createForwardLightingLambda(this);
  pipe->addStateLambda(L);
}
void PBRMaterial::addLightingLambda() {
  auto L = createForwardLightingLambda(this);
  _state_lambdas.push_back(L);
}

///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipelineFWD(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;

  ////////////////////////////////////////////////
  // set raster state
  ////////////////////////////////////////////////
  auto l_rsi = [this](const RenderContextInstData& RCID) {
    auto mut = const_cast<PBRMaterial*>(this);
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    //auto RSI     = context->RSI();
    //this->_rasterstate->setBlendingMacro(BlendingMacro::ADDITIVE);
    bool is_rendering_PROBE = RCFD->userPropertyAs<bool>("renderingPROBE"_crcu);

    ECullTest culltest = this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT;
    if(is_rendering_PROBE){
      culltest = ECullTest::OFF;
    }

    // Bump priority above the technique state block when the material wants
    // to override its cull (e.g. mtl.doubleSided / PROBE rendering).
    mut->_rasterstate->_priority = (this->_doubleSided || is_rendering_PROBE) ? (1 << 20) : 0;
    mut->_rasterstate->setCullTest(culltest);
    mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    if (this->_alphaMode == 2) { // BLEND
      mut->_rasterstate->setBlendingMacro(BlendingMacro::ALPHA);
      mut->_rasterstate->setWriteMaskZ(false); // transparent objects don't write depth
    } else {
      mut->_rasterstate->setWriteMaskZ(true);
    }
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(true);
  };
  ////////////////////////////////////////////////
  // ssao lambda
  ////////////////////////////////////////////////
  auto l_ssao = [this](const RenderContextInstData& RCID) {
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto FXI              = context->FXI();
      if( RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu ){
        //FXI->bindParamTexture(this->_paramSSAOTexture, nullptr );
        FXI->bindParamVect2(this->_parInvViewSize, fvec2(1,1) );
      }
      else {
          auto pbrcommon = RCFD->userPropertyAs<pbr::commonstuff_ptr_t>("PBR_COMMON"_crcu);
          auto ssaotexture = RCFD->userPropertyAs<texture_ptr_t>("SSAO_MAP"_crcu);
          auto depthtexture = RCFD->userPropertyAs<texture_ptr_t>("DEPTH_MAP"_crcu);

          //FXI->bindParamTexture(this->_paramMapDepth, depthtexture.get() );

          auto near_far = RCFD->userPropertyAs<fvec2>("NEAR_FAR"_crcu);
          auto ssaoDIM = RCFD->userPropertyAs<fvec2>("SSAO_DIM"_crcu);
          auto pmatrix = RCFD->userPropertyAs<fmtx4>("PMATRIX"_crcu);
          auto ipmatrix = RCFD->userPropertyAs<fmtx4>("IPMATRIX"_crcu);
          fvec2 ivpdim = fvec2(1.0f / ssaoDIM.x, 1.0f / ssaoDIM.y);
          FXI->bindParamTexture(this->_paramSSAOTexture, ssaotexture.get() );
          FXI->bindParamVect2(this->_parInvViewSize, ivpdim );
          FXI->bindParamVect2(this->_paramNearFar, near_far );
          FXI->bindParamMatrix(this->_paramP, pmatrix);
          FXI->bindParamMatrix(this->_paramIP, ipmatrix);

      }
  };
  /////////////////////////////////////////////////////////////
  // SSBO-SOURCED VERTICES (FWD_SSBO_CUSTOM): a first-class vertex variant, like rigid/
  // instanced/skinned — selected by permu._is_vertex_ssbo, picking the _tek_FWD_SSBO_CUSTOM
  // technique (compute-generated geometry pulled from an SSBO). Gets the SAME full forward
  // state (basic raster + lighting + ssao + MVP). Mono only for now. See project_fwd_ssbo_custom.
  /////////////////////////////////////////////////////////////
  // SSBO geometry x PER-INSTANCE MATRIX (FWD_SSBO_CUSTOM_INSTANCED): same SSBO-pull vertices, but each is
  // placed by storage_inst_mtx[gl_InstanceIndex] (the matrices SSBO is bound via the drawable's graphics
  // storage, like the geometry channels). One indirect draw, instanceCount instances. Checked BEFORE the
  // plain SSBO case so instanced+ssbo picks the instanced technique.
  if (permu._is_vertex_ssbo and permu._instanced and this->_tek_FWD_SSBO_CUSTOM_INSTANCED) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM_INSTANCED;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    return pipeline;
  }
  if (permu._is_vertex_ssbo and this->_tek_FWD_SSBO_CUSTOM) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_SSBO_CUSTOM;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    return pipeline;
  }
  /////////////////////////////////////////////////////////////
  // MATRICES-ONLY INSTANCED (FWD_CT_NM_IM_NI_MO): a first-class instanced variant that pulls per-
  // instance matrices from the dynamic storage_inst_mtx block (no per-instance color). Same forward
  // state as the other branches. See project_fwd_ssbo_custom (instancing sibling).
  /////////////////////////////////////////////////////////////
  if (permu._instanced_matrices_only and this->_tek_FWD_CT_NM_IM_NI_MO) {
    pipeline             = std::make_shared<FxPipeline>(permu);
    pipeline->_technique = this->_tek_FWD_CT_NM_IM_NI_MO;
    pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
    pipeline->addStateLambda(createBasicStateLambda(this));
    pipeline->addStateLambda(createForwardLightingLambda(this));
    pipeline->addStateLambda(l_rsi);
    pipeline->addStateLambda(l_ssao);
    pipeline->_material_ptr = (GfxMaterial*)this;
    pipeline->_rasterstate  = this->_rasterstate;
    return pipeline;
  }
  /////////////////////////////////////////////////////////////
  // STEREO
  /////////////////////////////////////////////////////////////
  if (permu._stereo and (not permu._vr_mono)) {
    ////////////////////////////// 
    // SKINNED
    ////////////////////////////// 
    if (permu._skinned) {
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_SK_IN_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_IN_ST;
        }
      } else {
        if (this->_tek_FWD_CT_NM_SK_NI_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_ST;
        }
      }

    }
    ////////////////////////////// 
    // RIGID
    ////////////////////////////// 
    else {                  
      if (permu._instanced) { // instanced
        if (this->_tek_FWD_CT_NM_RI_IN_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_ST;
        }
      } else { // not instanced
        if(permu._vr_mono and this->_tek_FWD_CT_NM_SK_NI_MO ){
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_MO;
        }
        else if (this->_tek_FWD_CT_NM_RI_NI_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_ST;
        }
      }
    }
    if(pipeline){
        pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
        pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
        pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda(createForwardLightingLambda(this));
        pipeline->addStateLambda(l_rsi);
    }
  }
  /////////////////////////////////////////////////////////////
  // FORWARD_PBR::MONO
  /////////////////////////////////////////////////////////////
  else {
    if (permu._skinned) {
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_SK_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_IN_MO;
          //printf( "got fwdtek FWD_CT_NM_SK_IN_MO\n");
        }
      } else { // not instanced
        if (this->_tek_FWD_CT_NM_SK_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_MO;
          //printf( "got fwdtek FWD_CT_NM_SK_NI_MO\n");
        }
      }
    } else { // not skinned
      if (permu._instanced) {
        if (permu._has_vtxcolors) {
          if (permu._is_alpha && this->_tek_FWD_CV_NM_RI_IN_MO_ALPHA) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_IN_MO_ALPHA;
          } else if (this->_tek_FWD_CV_NM_RI_IN_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_IN_MO;
          }
        } else if (this->_tek_FWD_CT_NM_RI_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_MO;
        }
      } else {
        if( permu._has_vtxcolors ){
          if (permu._is_alpha && this->_tek_FWD_CV_NM_RI_NI_MO_ALPHA) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_MO_ALPHA;
          }
          else if (this->_tek_FWD_CV_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_MO;
          }
        }
        else{
          if (this->_tek_FWD_CT_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_MO;
            //printf( "got fwdtek FWD_CT_NM_RI_NI_MO\n");
          }
        }
      }
    }
    if(pipeline){
      if(permu._vr_mono){
        pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Left"_crcsh);
      }
      else{
        pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
      }
      pipeline->addStateLambda(createBasicStateLambda(this));
      pipeline->addStateLambda(createForwardLightingLambda(this));
      pipeline->addStateLambda(l_rsi);
      pipeline->addStateLambda(l_ssao);
    }
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
  }
  // OrkAssert(pipeline->_technique != nullptr);
  return pipeline;
}

} // namespace ork::lev2
