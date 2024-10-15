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

static logchannel_ptr_t logchan_pbr_fwd = logger()->createChannel("mtlpbrFWD", fvec3(0.8, 0.8, 0.1), true);

FxPipeline::statelambda_t createForwardLightingLambda(const PBRMaterial* mtl) {

  auto L = [mtl](const RenderContextInstData& RCID, int ipass) {

    //printf( "LIGHTINGLAMBDA\n");
    auto RCFD             = RCID.rcfd();
    auto context    = RCFD->GetTarget();
    auto FXI        = context->FXI();
    bool is_skinned = RCID._isSkinned;

    ///////////////////////////////////////////////////////////////////////////
    // retrieve global RCFD state for PBR materials
    ///////////////////////////////////////////////////////////////////////////

    auto enumlights = RCFD->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    bool is_rendering_PROBE = RCFD->userPropertyAs<bool>("renderingPROBE"_crcu);
    bool have_PROBES = RCFD->userPropertyAs<bool>("havePROBES"_crcu);
    bool should_bind_probes = (have_PROBES);
    bool is_depth_prepass = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;

    ///////////////////////////////////////////////////////////////////////////
    // we are not lighting for depth prepass, so NOP
    ///////////////////////////////////////////////////////////////////////////

    if (is_depth_prepass)
      return;

    ///////////////////////////////////////////////////////////////////////////
    // build lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    // logchan_pbr_fwd->log("fwd: all lights count<%zu>", enumlights->_alllights.size());

    int num_untextured_pointlights = enumlights->_untexturedpointlights.size();
    int num_texspotlights          = 0;

    auto pl_buffer = PBRMaterial::pointLightDataBuffer(context);
    // size_t map_length = 16 * (sizeof(fvec4) + sizeof(fvec4) + sizeof(float));
    auto pl_mapped = FXI->mapParamBuffer(pl_buffer, 0, pl_buffer->_length);

    size_t i32_stride  = sizeof(int32_t);
    size_t f32_stride  = sizeof(float);
    size_t vec4_stride = sizeof(fvec4);
    size_t mat4_stride = sizeof(fmtx4);

    size_t base_color    = 0;
    size_t base_sizbias  = base_color + vec4_stride * 64;
    size_t base_position = base_sizbias + vec4_stride * 64;
    size_t base_shmtx    = base_position + vec4_stride * 64;

    if (0) {
      printf("base_color<%zu>\n", base_color);
      printf("base_sizbias<%zu>\n", base_sizbias);
      printf("base_position<%zu>\n", base_position);
      printf("base_shmtx<%zu>\n", base_shmtx);
    }
    // 16*(16+16+8) = 16*40 = 640

    size_t index = 0;
    for (auto light : enumlights->_untexturedpointlights) {
      auto C                                                    = fvec4(light->color(), light->intensity());
      auto P                                                    = light->worldPosition();
      float R                                                   = light->radius();
      size_t v4_offset                                          = index * vec4_stride;
      pl_mapped->ref<fvec4>(base_color + v4_offset)             = C;
      pl_mapped->ref<fvec4>(base_sizbias + v4_offset)           = fvec4(R, 0, 0, 1);
      pl_mapped->ref<fvec4>(base_position + v4_offset)          = P;
      pl_mapped->ref<fmtx4>(base_shmtx + (index * mat4_stride)) = fmtx4();
      index++;
    }

    texture_rawlist_t texlist;

    for (auto item : enumlights->_tex2spotlightmap) {
      for (auto light : item.second) {
        auto irr = light->_irradianceCookie;

        auto C    = fvec4(light->color(), light->intensity());
        auto P    = light->worldMatrix().translation();
        float R   = light->_spdata->GetRange();
        float B   = light->shadowDepthBias();
        float SMS = light->_spdata->shadowMapSize();

        // printf( "C<%g %g %g %g>\n", C.x, C.y, C.z, C.w );
        // printf( "P<%g %g %g>\n", P.x, P.y, P.z );
        // printf( "R<%f> B<%f> SMS<%f>\n", R, B, SMS );

        size_t v4_offset                                          = index * vec4_stride;
        pl_mapped->ref<fvec4>(base_color + v4_offset)             = C;
        pl_mapped->ref<fvec4>(base_sizbias + v4_offset)           = fvec4(R, B, SMS, 1);
        pl_mapped->ref<fvec4>(base_position + v4_offset)          = P;
        pl_mapped->ref<fmtx4>(base_shmtx + (index * mat4_stride)) = light->shadowMatrix();
        index++;
        num_texspotlights++;

        auto specmap  = irr->_filtenvSpecularMap.get();
        auto depthmap = light->_depthRTG->_depthBuffer->_texture.get();

        texlist.push_back(specmap);
        texlist.push_back(depthmap);
      }
    }

    // printf( "texlistsize<%d>\n", texlist.size() );
    pl_mapped->unmap();

    ///////////////////////////////////////////////////////////////////////////
    // bind lighting UBO
    ///////////////////////////////////////////////////////////////////////////

    if (mtl->_parUnTexPointLightsCount)
      FXI->BindParamInt(mtl->_parUnTexPointLightsCount, num_untextured_pointlights);
    if (mtl->_parUnTexPointLightsData) {
      // printf( "binding lighting UBO\n");
      FXI->bindParamBlockBuffer(mtl->_parUnTexPointLightsData, pl_buffer);
    }

    ///////////////////////////////////////////////////////////////////////////
    // bind spotlight cookies
    ///////////////////////////////////////////////////////////////////////////
 
     if (mtl->_parTexSpotLightsCount) {
      //printf("binding texspotlights<%d>\n", num_texspotlights);
      FXI->BindParamInt(mtl->_parTexSpotLightsCount, num_texspotlights);
      // FXI->bindParamTextureList(mtl->_parLightCookies, texlist );
      if (texlist.size() > 0) {
        FXI->BindParamCTex(mtl->_parLightCookie0, texlist[0]);
      }
      if (texlist.size() > 1) {
        FXI->BindParamCTex(mtl->_parLightCookie1, texlist[1]);
      }
      if (texlist.size() > 2) {
        FXI->BindParamCTex(mtl->_parLightCookie2, texlist[2]);
      }
      if (texlist.size() > 3) {
        FXI->BindParamCTex(mtl->_parLightCookie3, texlist[3]);
      }
      
    }
    
    ///////////////////////////////////////////////////////////////////////////
    // bind light/environment probes
    ///////////////////////////////////////////////////////////////////////////

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
      FXI->BindParamCTex(mtl->_parProbeReflection, probe_tex.get() );
      FXI->BindParamCTex(mtl->_parProbeIrradiance, probe_tex.get() );


    }
    else{
      OrkAssert(mtl->_texCubeBlack);
      //printf( "NOT BINDING PROBES black<%p>!\n", mtl->_texCubeBlack.get() );
      FXI->BindParamCTex(mtl->_parProbeReflection, mtl->_texCubeBlack.get() );
      FXI->BindParamCTex(mtl->_parProbeIrradiance, mtl->_texCubeBlack.get() );
    }

    ///////////////////////////////////////////////////////////////////////////

    auto modcolor = context->RefModColor();
    FXI->BindParamVect4(mtl->_parModColor, modcolor * mtl->_baseColor);
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
  auto l_rsi = [this](const RenderContextInstData& RCID, int ipass) {
    auto mut = const_cast<PBRMaterial*>(this);
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto RSI     = context->RSI();
    //this->_rasterstate.SetBlending(Blending::ADDITIVE);
    mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
    mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate.SetZWriteMask(true);
    mut->_rasterstate.SetRGBAWriteMask(true, true);
    RSI->BindRasterState(this->_rasterstate);
  };
  ////////////////////////////////////////////////
  // ssao lambda
  ////////////////////////////////////////////////
  auto l_ssao = [this](const RenderContextInstData& RCID, int ipass) {
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto FXI              = context->FXI();
      if( RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu ){
        //FXI->BindParamCTex(this->_paramSSAOTexture, nullptr );
        FXI->BindParamVect2(this->_parInvViewSize, fvec2(1,1) );
      }
      else {
          auto pbrcommon = RCFD->userPropertyAs<pbr::commonstuff_ptr_t>("PBR_COMMON"_crcu);
          auto ssaotexture = RCFD->userPropertyAs<texture_ptr_t>("SSAO_MAP"_crcu);
          auto depthtexture = RCFD->userPropertyAs<texture_ptr_t>("DEPTH_MAP"_crcu);

          FXI->BindParamCTex(this->_paramMapDepth, depthtexture.get() );

          if(RCFD->hasUserProperty("LINEAR_DEPTH_MAP"_crcu)){
            auto lindepthtexture = RCFD->userPropertyAs<texture_ptr_t>("LINEAR_DEPTH_MAP"_crcu);
            auto near_far = RCFD->userPropertyAs<fvec2>("NEAR_FAR"_crcu);
            auto ssaoDIM = RCFD->userPropertyAs<fvec2>("SSAO_DIM"_crcu);
            auto ssaoPOWER = RCFD->userPropertyAs<float>("SSAO_POWER"_crcu);
            auto ssaoWEIGHT = RCFD->userPropertyAs<float>("SSAO_WEIGHT"_crcu);
            auto pmatrix = RCFD->userPropertyAs<fmtx4>("PMATRIX"_crcu);
            auto ipmatrix = RCFD->userPropertyAs<fmtx4>("IPMATRIX"_crcu);
            auto kernel = RCFD->userPropertyAs<texture_ptr_t>("SSAO_KERNEL"_crcu);
            auto scrnoise = RCFD->userPropertyAs<texture_ptr_t>("SSAO_SCRNOISE"_crcu);
            fvec2 ivpdim = fvec2(1.0f / ssaoDIM.x, 1.0f / ssaoDIM.y);
            FXI->BindParamCTex(this->_paramMapLinearDepth, lindepthtexture.get() );
            FXI->BindParamCTex(this->_paramSSAOTexture, ssaotexture.get() );
            FXI->BindParamCTex(this->_paramSSAOKernel, kernel.get());
            FXI->BindParamCTex(this->_paramSSAOScrNoise, scrnoise.get());

            FXI->BindParamFloat(this->_paramSSAOPower, ssaoPOWER );
            FXI->BindParamFloat(this->_paramSSAOWeight, ssaoWEIGHT );
            FXI->BindParamVect2(this->_parInvViewSize, ivpdim );
            FXI->BindParamVect2(this->_paramNearFar, near_far );
            FXI->BindParamInt(this->_paramSSAONumSamples, pbrcommon->_ssaoNumSamples);
            FXI->BindParamInt(this->_paramSSAONumSteps, pbrcommon->_ssaoNumSteps);
            FXI->BindParamFloat(this->_paramSSAOBias, pbrcommon->_ssaoBias);
            FXI->BindParamFloat(this->_paramSSAORadius, pbrcommon->_ssaoRadius);
            FXI->BindParamFloat(this->_paramSSAOWeight, pbrcommon->_ssaoWeight);
            FXI->BindParamFloat(this->_paramSSAOPower, pbrcommon->_ssaoPower);
            FXI->BindParamMatrix(this->_paramP, pmatrix);
            FXI->BindParamMatrix(this->_paramIP, ipmatrix);
            pmatrix.dump("PMATRIX");
            printf( "near<%f> far<%f>\n", near_far.x, near_far.y );
            printf( "ivpdim<%f %f>\n", ivpdim.x, ivpdim.y );
          }



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
          printf( "got fwdtek FWD_CT_NM_SK_IN_MO\n");
        }
      } else { // not instanced
        if (this->_tek_FWD_CT_NM_SK_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_MO;
          printf( "got fwdtek FWD_CT_NM_SK_NI_MO\n");
        }
      }
    } else { // not skinned
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_RI_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_MO;
          printf( "got fwdtek FWD_CT_NM_RI_IN_MO\n");
        }
      } else {
        if( permu._has_vtxcolors ){
          if (this->_tek_FWD_CV_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CV_NM_RI_NI_MO;
            printf( "got fwdtek FWD_CV_NM_SK_NI_MO\n");
          }
        }
        else{
          if (this->_tek_FWD_CT_NM_RI_NI_MO) {
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_MO;
            printf( "got fwdtek FWD_CT_NM_RI_NI_MO\n");
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

  // OrkAssert(pipeline->_technique != nullptr);
  return pipeline;
}

} // namespace ork::lev2
