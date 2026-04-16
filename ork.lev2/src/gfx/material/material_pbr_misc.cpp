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
///////////////////////////////////////////////////////////////////////////////

FxPipeline::statelambda_t createForwardLightingLambda(const PBRMaterial* mtl);

fxpipeline_ptr_t PBRMaterial::_createFxPipelineSKY(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  auto basic_lambda  = createBasicStateLambda(this);
  auto skybox_lambda = [this, basic_lambda](const RenderContextInstData& RCID) {
    auto mut       = (PBRMaterial*)this;
    auto RCFD      = RCID.rcfd();
    auto context   = RCFD->GetTarget();
    auto FXI       = context->FXI();
    auto MTXI      = context->MTXI();
    auto pbrcommon = RCFD->_pbrcommon;
    auto envtex    = pbrcommon->envSpecularTexture();

    FXI->bindParamTextureArray(this->_parMapSpecularEnv, envtex.get());

    basic_lambda(RCID);
    mut->_rasterstate->setCullTest(ECullTest::OFF);
    mut->_rasterstate->setWriteMaskZ(false);
    mut->_rasterstate->setDepthTest(EDepthTest::LEQUALS);
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(true);
    //RSI->BindRasterState(this->_rasterstate);
  };
  //////////////////////////////////////////////////////////
  OrkAssert(permu._instanced == false);
  OrkAssert(permu._skinned == false);
  //////////////////////////////////////////////////////////
  if (permu._stereo and (not permu._vr_mono) and this->_tek_FWD_SKYBOX_ST) {
    auto pipeline_stereo        = std::make_shared<FxPipeline>(permu);
    pipeline_stereo->_technique = this->_tek_FWD_SKYBOX_ST;
    pipeline_stereo->bindParam(this->_paramIVPL, "RCFD_Camera_IVP_Left"_crcsh);
    pipeline_stereo->bindParam(this->_paramIVPR, "RCFD_Camera_IVP_Right"_crcsh);
    pipeline_stereo->addStateLambda(skybox_lambda);
    pipeline                   = pipeline_stereo;
  } else if (this->_tek_FWD_SKYBOX_MO) {
    auto pipeline_mono        = std::make_shared<FxPipeline>(permu);
    pipeline_mono->_technique = this->_tek_FWD_SKYBOX_MO;
    pipeline_mono->bindParam(this->_paramIVP, "RCFD_Camera_IVP_NoTrans_Mono"_crcsh);
    pipeline_mono->addStateLambda(skybox_lambda);
    pipeline                   = pipeline_mono;
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
  }
  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////

fxpipeline_ptr_t PBRMaterial::_createFxPipelineVTX(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  auto no_cull_stateblock = [this](const RenderContextInstData& RCID) {
    auto mut   = (PBRMaterial*)this;
    auto RCFD    = RCID.rcfd();
    auto context = RCFD->GetTarget();
    auto FXI     = context->FXI();
    auto MTXI    = context->MTXI();
    //auto RSI     = context->RSI();
    mut->_rasterstate->setCullTest(ECullTest::OFF);
    mut->_rasterstate->setDepthTest(EDepthTest::OFF);
    mut->_rasterstate->setWriteMaskZ(true);
    mut->_rasterstate->setWriteMaskRGB(true);
    mut->_rasterstate->setWriteMaskA(false);
  };
  switch (permu._rendering_model) {
    case "FORWARD_PBR"_crcu: {
      if (not permu._instanced and not permu._skinned and not permu._stereo) {
        if (this->_tek_FWD_CV_EMI_RI_NI_MO) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_FWD_CV_EMI_RI_NI_MO;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
          pipeline->addStateLambda(createForwardLightingLambda(this));
          pipeline->addStateLambda(createBasicStateLambda(this));
          pipeline->addStateLambda(no_cull_stateblock);
          OrkAssert(pipeline->_technique != nullptr);
        }
      }
      break;
    }
    case "PICKING"_crcu: {
      if (not permu._instanced and not permu._skinned and not permu._stereo) {
        if (this->_tek_PIK_RI_NI) {
          pipeline             = std::make_shared<FxPipeline>(permu);
          pipeline->_technique = this->_tek_PIK_RI_NI;
          pipeline->bindParam(this->_paramMVP, "RCFD_Camera_Pick"_crcsh);
          pipeline->bindParam(this->_paramM, "RCFD_M"_crcsh);
          pipeline->bindParam(this->_paramMROT, "RCFD_Model_Rot"_crcsh);
          pipeline->bindParam(this->_parPickID, "RCID_PickID"_crcsh);
          OrkAssert(pipeline->_technique != nullptr);
        }
      }
      break;
    }
    default:
      break;
  }
  if(pipeline){
    pipeline->_material_ptr = (GfxMaterial*) this;
    pipeline->_rasterstate = this->_rasterstate;
  }
  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
