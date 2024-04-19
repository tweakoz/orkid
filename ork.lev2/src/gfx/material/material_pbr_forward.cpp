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
    auto RCFD             = RCID._RCFD;
    bool is_depth_prepass = RCFD->_renderingmodel._modelID == "DEPTH_PREPASS"_crcu;
    if (is_depth_prepass)
      return;

    bool is_skinned = RCID._isSkinned;
    // printf( "lighting_lambda skinned<%d>\n", int(is_skinned));

    auto enumlights = RCFD->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    auto context    = RCFD->GetTarget();
    auto FXI        = context->FXI();
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

    if (mtl->_parTexSpotLightsCount) {
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
      if (texlist.size() > 4) {
        FXI->BindParamCTex(mtl->_parLightCookie4, texlist[4]);
      }
      if (texlist.size() > 5) {
        FXI->BindParamCTex(mtl->_parLightCookie5, texlist[5]);
      }
      if (texlist.size() > 6) {
        FXI->BindParamCTex(mtl->_parLightCookie6, texlist[6]);
      }
      if (texlist.size() > 7) {
        FXI->BindParamCTex(mtl->_parLightCookie7, texlist[7]);
      }
    }

    if (mtl->_parUnTexPointLightsCount)
      FXI->BindParamInt(mtl->_parUnTexPointLightsCount, num_untextured_pointlights);
    if (mtl->_parUnTexPointLightsData) {
      // printf( "binding lighting UBO\n");
      FXI->bindParamBlockBuffer(mtl->_parUnTexPointLightsData, pl_buffer);
    }

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

fxpipeline_ptr_t PBRMaterial::_createFxPipelineFWD(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;

  ////////////////////////////////////////////////
  // setup forward lighting
  ////////////////////////////////////////////////

  ////////////////////////////////////////////////
  // set raster state
  ////////////////////////////////////////////////
  auto rsi_lambda = [this](const RenderContextInstData& RCID, int ipass) {
    auto mut = const_cast<PBRMaterial*>(this);
    auto RCFD    = RCID._RCFD;
    auto context = RCFD->GetTarget();
    auto RSI     = context->RSI();
    //this->_rasterstate.SetBlending(Blending::ADDITIVE);
    mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
    mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate.SetZWriteMask(true);
    mut->_rasterstate.SetRGBAWriteMask(true, true);
    RSI->BindRasterState(this->_rasterstate);
  };
  /////////////////////////////////////////////////////////////
  if (permu._stereo) {
    if (permu._skinned) {
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_SK_IN_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_IN_ST;
          pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
          pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      } else {
        if (this->_tek_FWD_CT_NM_SK_NI_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_ST;
          pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
          pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      }

    } else {                  // not skinned
      if (permu._instanced) { // instanced
        if (this->_tek_FWD_CT_NM_RI_IN_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_ST;
          pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
          pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      } else { // not instanced
        if (this->_tek_FWD_CT_NM_RI_NI_ST) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_ST;
          pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
          pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      }
    }
  }
  // FORWARD_PBR::STEREO
  /////////////////////////////////////////////////////////////
  // FORWARD_PBR::MONO
  else {
    if (permu._skinned) {
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_SK_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_IN_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      } else { // not instanced
        if (this->_tek_FWD_CT_NM_SK_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_SK_NI_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      }
    } else { // not skinned
      if (permu._instanced) {
        if (this->_tek_FWD_CT_NM_RI_IN_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_IN_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      } else {
        if (this->_tek_FWD_CT_NM_RI_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CT_NM_RI_NI_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(rsi_lambda);
        }
      }
    }
  }

  // OrkAssert(pipeline->_technique != nullptr);
  return pipeline;
}

} // namespace ork::lev2