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

static logchannel_ptr_t logchan_pbr_unl = logger()->createChannel("mtlpbrDPP", fvec3(0.8, 0.8, 0.1), true);

fxpipeline_ptr_t PBRMaterial::_createFxPipelineDPP(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;

  if ((not permu._instanced) and (not permu._skinned)) {
    if (permu._stereo) {
      if (this->_tek_FWD_DEPTHPREPASS_RI_NI_ST) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_DEPTHPREPASS_RI_NI_ST;
        pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
        pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
        pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
          auto mut = const_cast<PBRMaterial*>(this);
          auto RCFD    = RCID.rcfd();
          auto context = RCFD->GetTarget();
          auto FXI     = context->FXI();
          auto MTXI    = context->MTXI();
          auto RSI     = context->RSI();
          mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
          mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
          mut->_rasterstate.SetZWriteMask(true);
          mut->_rasterstate.SetRGBAWriteMask(false, false);
          RSI->BindRasterState(this->_rasterstate);
        });
      }
    } else {
        
      if (this->_tek_FWD_DEPTHPREPASS_RI_NI_MO) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_DEPTHPREPASS_RI_NI_MO;
        pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
        pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
          auto mut = const_cast<PBRMaterial*>(this);
          auto RCFD    = RCID.rcfd();
          auto context = RCFD->GetTarget();
          auto FXI     = context->FXI();
          auto MTXI    = context->MTXI();
          auto RSI     = context->RSI();
          mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
          mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
          mut->_rasterstate.SetZWriteMask(true);
          mut->_rasterstate.SetRGBAWriteMask(false, false);
          RSI->BindRasterState(this->_rasterstate);
        });
      }
      else{
        logchan_pbr_unl->log( "mtl<%s> NO _tek_FWD_DEPTHPREPASS_RI_NI_MO\n", mMaterialName.c_str() );
      
      }
    }
  } else if (not permu._instanced and permu._skinned) {
    if (permu._stereo) {
      if (this->_tek_FWD_DEPTHPREPASS_SK_NI_ST) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_DEPTHPREPASS_SK_NI_ST;
        pipeline->bindParam(this->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
        pipeline->bindParam(this->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
        pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
          auto mut = const_cast<PBRMaterial*>(this);
          auto RCFD    = RCID.rcfd();
          auto context = RCFD->GetTarget();
          auto FXI     = context->FXI();
          auto MTXI    = context->MTXI();
          auto RSI     = context->RSI();
          mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
          mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
          mut->_rasterstate.SetZWriteMask(true);
          mut->_rasterstate.SetRGBAWriteMask(false, false);
          RSI->BindRasterState(this->_rasterstate);
        });
      }
    } else {
      if (this->_tek_FWD_DEPTHPREPASS_SK_NI_MO) {
        pipeline             = std::make_shared<FxPipeline>(permu);
        pipeline->_technique = this->_tek_FWD_DEPTHPREPASS_SK_NI_MO;
        pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
        pipeline->addStateLambda(createBasicStateLambda(this));
        pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
          auto mut = const_cast<PBRMaterial*>(this);
          auto RCFD    = RCID.rcfd();
          auto context = RCFD->GetTarget();
          auto FXI     = context->FXI();
          auto MTXI    = context->MTXI();
          auto RSI     = context->RSI();
          mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
          mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
          mut->_rasterstate.SetZWriteMask(true);
          mut->_rasterstate.SetRGBAWriteMask(false, false);
          RSI->BindRasterState(this->_rasterstate);
        });
      }
    }
  } else if (permu._instanced and not permu._skinned) {
    if (this->_tek_FWD_DEPTHPREPASS_RI_IN_MO) {
      pipeline             = std::make_shared<FxPipeline>(permu);
      pipeline->_technique = this->_tek_FWD_DEPTHPREPASS_RI_IN_MO;
      pipeline->bindParam(this->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
      pipeline->addStateLambda(createBasicStateLambda(this));
      pipeline->addStateLambda([this](const RenderContextInstData& RCID, int ipass) {
        auto mut = const_cast<PBRMaterial*>(this);
        auto RCFD    = RCID.rcfd();
        auto context = RCFD->GetTarget();
        auto FXI     = context->FXI();
        auto MTXI    = context->MTXI();
        auto RSI     = context->RSI();
        mut->_rasterstate.SetCullTest(this->_doubleSided ? ECullTest::OFF : ECullTest::PASS_FRONT);
        mut->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
        mut->_rasterstate.SetZWriteMask(true);
        mut->_rasterstate.SetRGBAWriteMask(false, false);
        RSI->BindRasterState(this->_rasterstate);
      });
    }
  }
  if(nullptr==pipeline){
    logchan_pbr_unl->log( "mtl<%s> NO DEPTH PREPASS\n", mMaterialName.c_str() );
  }
  return pipeline;
}

} // namespace ork::lev2
