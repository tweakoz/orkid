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

    //printf( "LIGHTINGLAMBDA\n");
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

    ///////////////////////////////////////////////////////////////////////////
    // bind lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    if (mtl->_parUnTexPointLightsCount) {
      FXI->bindParamInt(mtl->_parUnTexPointLightsCount, enumlights->_num_active_untextured_pointlights);
    }
    if (mtl->_parForwardLightBlock) {
      auto pl_buffer = PBRMaterial::lightingDataBuffer(context);
      OrkAssert(false); // update to SSBO interface
      //FXI->bindUniformBuffer(mtl->_parForwardLightBlock, pl_buffer);
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind spotlight cookies
    ///////////////////////////////////////////////////////////////////////////
 
     if (mtl->_parTexSpotLightsCount) {
      FXI->bindParamInt(mtl->_parTexSpotLightsCount, enumlights->_num_active_texspotlights);
      FXI->bindParamTextureArray(mtl->_parLightDepthCookies, lmgr->_cookies_spot_depth.get() );
      FXI->bindParamTextureArray(mtl->_parLightColorCookies, lmgr->_cookies_spot_color.get() );
    }
    FXI->bindParamTextureArray( mtl->_paramMapCNMREA, mtl->_texArrayCNMREA.get() );

    ///////////////////////////////////////////////////////////////////////////
    // bind light/environment probes
    ///////////////////////////////////////////////////////////////////////////

    //printf("should_bind_probes<%d> is_rendering_PROBE<%d>\n", int(should_bind_probes), int(is_rendering_PROBE));
    if(should_bind_probes and (not is_rendering_PROBE)){
      size_t num_probes = enumlights->_lightprobes.size();

      // technically here we should only bind a set of probes 
      // that are relevant to the current rendered object
      // and bind the weight of each probe
      // for now we will just bind all probes

      auto probe_0 = enumlights->_lightprobes[0];
      auto probe_tex = probe_0->_cubeTexture;

      //printf( "BINDING PROBES!  count<%d>\n", num_probes );
      //printf( "binding probetex<%p>\n", probe_tex.get() );
      FXI->bindParamTexture(mtl->_parProbeReflection, probe_tex.get() );
      FXI->bindParamTexture(mtl->_parProbeRadiance, probe_tex.get() );


    }
    else{
      //printf( "NOT BINDING PROBES black<%p>!\n", mtl->_texCubeBlack.get() );
      FXI->bindParamTexture(mtl->_parProbeReflection, pbrcommon->_texCubeBlack.get() );
      FXI->bindParamTexture(mtl->_parProbeRadiance, pbrcommon->_texCubeBlack.get() );
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

    auto modcolor = context->RefModColor();
    FXI->bindParamVect4(mtl->_parModColor, modcolor * mtl->_baseColor);
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

    mut->_rasterstate->setCullTest(culltest);
    mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate->setWriteMaskZ(true);
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(true);
    //RSI->BindRasterState(this->_rasterstate);
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

          FXI->bindParamTexture(this->_paramMapDepth, depthtexture.get() );

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
        if (this->_tek_FWD_CT_NM_RI_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_MO;
          //printf( "got fwdtek FWD_CT_NM_RI_IN_MO\n");
        }
      } else {
        if( permu._has_vtxcolors ){
          if (this->_tek_FWD_CV_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_MO;
            //printf( "got fwdtek FWD_CV_NM_SK_NI_MO\n");
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
