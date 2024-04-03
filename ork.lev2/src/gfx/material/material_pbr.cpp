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

ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbr = logger()->createChannel("mtlpbr", fvec3(0.8, 0.8, 0.1), true);

//////////////////////////////////////////////////////

struct GlobalDefaultMaterial{
  GlobalDefaultMaterial(Context* ctx){
     _material = std::make_shared<PBRMaterial>(ctx);
  }
  pbrmaterial_ptr_t _material;
};

pbrmaterial_ptr_t default3DMaterial(Context* ctx) {
  static GlobalDefaultMaterial _gdm(ctx);
  return _gdm._material;
}

//////////////////////////////////////////////////////

static fxpipeline_ptr_t _createFxPipeline(const FxPipelinePermutation& permu,const PBRMaterial*mtl);
using cache_impl_t = FxPipelineCacheImpl<PBRMaterial>;

using pbrcache_impl_ptr_t = std::shared_ptr<cache_impl_t>;

static pbrcache_impl_ptr_t _getpbrcache(){
  static pbrcache_impl_ptr_t _gcache = std::make_shared<cache_impl_t>();
  return _gcache;
}

////////////////////////////////////////////

FxPipeline::statelambda_t createBasicStateLambda(const PBRMaterial* mtl){
  return [mtl](const RenderContextInstData& RCID, int ipass) {
    auto context          = RCID._RCFD->GetTarget();
    auto MTXI             = context->MTXI();
    auto FXI              = context->FXI();
    auto RSI              = context->RSI();
    const auto& CPD       = RCID._RCFD->topCPD();
    const auto& RCFDPROPS = RCID._RCFD->userProperties();
    bool is_picking       = CPD.isPicking();
    bool is_stereo        = CPD.isStereoOnePass();
    auto pbrcommon        = RCID._RCFD->_pbrcommon;

    float num_mips = pbrcommon->envSpecularTexture()->_num_mips;

    FXI->BindParamVect3(mtl->_paramAmbientLevel, pbrcommon->_ambientLevel);
    FXI->BindParamFloat(mtl->_paramSpecularLevel, pbrcommon->_specularLevel);
    FXI->BindParamFloat(mtl->_parSpecularMipBias, pbrcommon->_specularMipBias);
    FXI->BindParamFloat(mtl->_paramDiffuseLevel, pbrcommon->_diffuseLevel);
    FXI->BindParamFloat(mtl->_paramSkyboxLevel, pbrcommon->_skyboxLevel);
    FXI->BindParamCTex(mtl->_parMapSpecularEnv, pbrcommon->envSpecularTexture().get());
    FXI->BindParamCTex(mtl->_parMapDiffuseEnv, pbrcommon->envDiffuseTexture().get());
    FXI->BindParamCTex(mtl->_parMapBrdfIntegration, pbrcommon->_brdfIntegrationMap.get());
    FXI->BindParamFloat(mtl->_parEnvironmentMipBias, pbrcommon->_environmentMipBias);
    FXI->BindParamFloat(mtl->_parEnvironmentMipScale, pbrcommon->_environmentMipScale*num_mips);
    FXI->BindParamFloat(mtl->_parDepthFogDistance, pbrcommon->_depthFogDistance);
    FXI->BindParamFloat(mtl->_parDepthFogPower, pbrcommon->_depthFogPower);

    auto worldmatrix = RCID.worldMatrix();

    auto stereocams = CPD._stereoCameraMatrices;
    auto monocams   = CPD._cameraMatrices;

    FXI->BindParamMatrix(mtl->_paramM, worldmatrix);

    if(stereocams){
      fmtx4 vrroot;
      auto vrrootprop = RCID._RCFD->getUserProperty("vrroot"_crc);
      if (auto as_mtx = vrrootprop.tryAs<fmtx4>()) {
        vrroot = as_mtx.value();
      }

      OrkAssert(mtl->_paramVPL);
      OrkAssert(mtl->_paramVPR);

      auto VL = stereocams->VL();
      auto VR = stereocams->VR();
      auto VPL = stereocams->VPL();
      auto VPR = stereocams->VPR();
      if(mtl->_paramVL){
        FXI->BindParamMatrix(mtl->_paramVL, VL);
      }
      if(mtl->_paramVR){
        FXI->BindParamMatrix(mtl->_paramVR, VR);
      }
      FXI->BindParamMatrix(mtl->_paramVPL, VPL);
      FXI->BindParamMatrix(mtl->_paramVPR, VPR);
      FXI->BindParamMatrix(mtl->_paramMVPL, stereocams->MVPL(vrroot*worldmatrix));
      FXI->BindParamMatrix(mtl->_paramMVPR, stereocams->MVPR(vrroot*worldmatrix));

      FXI->BindParamVect3(mtl->_paramEyePostionL, VL.inverse().translation());
      FXI->BindParamVect3(mtl->_paramEyePostionR, VR.inverse().translation());

    }
    else if (monocams) {
      auto eye_pos = monocams->_vmatrix.inverse().translation();
      FXI->BindParamVect3(mtl->_paramEyePostion, eye_pos);
      FXI->BindParamMatrix(mtl->_paramMVP, monocams->MVPMONO(worldmatrix));

      auto VP = monocams->VPMONO();
      FXI->BindParamMatrix(mtl->_paramVP, VP);
      FXI->BindParamMatrix(mtl->_paramIVP, VP.inverse());
    }
  };

}

FxPipeline::statelambda_t  createLightingLambda(const PBRMaterial* mtl){

  auto L = [mtl](const RenderContextInstData& RCID, int ipass) {

    auto RCFD       = RCID._RCFD;
    bool is_depth_prepass = RCFD->_renderingmodel._modelID=="DEPTH_PREPASS"_crcu;
    if(is_depth_prepass)
      return;

    auto enumlights = RCFD->userPropertyAs<enumeratedlights_ptr_t>("enumeratedlights"_crcu);
    auto context    = RCFD->GetTarget();
    auto FXI        = context->FXI();
     //logchan_pbr->log("fwd: all lights count<%zu>", enumlights->_alllights.size());

    int num_untextured_pointlights = enumlights->_untexturedpointlights.size();
    //printf( "num_untextured_pointlights<%d>\n", num_untextured_pointlights );
    auto pl_buffer = PBRMaterial::pointLightDataBuffer(context);
    size_t map_length = 16 * (sizeof(fvec4) + sizeof(fvec4) + sizeof(float));
    auto pl_mapped = FXI->mapParamBuffer(pl_buffer, 0, pl_buffer->_length);

    size_t f32_stride    = sizeof(float);
    size_t vec4_stride   = sizeof(fvec4);
    size_t base_color    = 0;
    size_t base_position = base_color + vec4_stride * 64;
    size_t base_radius   = base_position + vec4_stride * 64;
    // 16*(16+16+8) = 16*40 = 640

    size_t index = 0;
    for (auto light : enumlights->_untexturedpointlights) {
      auto C = fvec4(light->color(), light->intensity());
      auto P = light->worldPosition();
      float R = light->radius();
      pl_mapped->ref<fvec4>(base_color + (index * vec4_stride))    = C;
      pl_mapped->ref<fvec4>(base_position + (index * vec4_stride)) = P;
      pl_mapped->ref<float>(base_radius + (index * f32_stride))    = R;
      //logchan_pbr->log("doing light<%p> color<%g %g %g> pos<%g %g %g> R<%g>", (void*) light, C.x, C.y, C.z, P.x, P.y, P.z, R);
      index++;
    }

    pl_mapped->unmap();

    if(mtl->_parUnTexPointLightsCount)
      FXI->BindParamInt(mtl->_parUnTexPointLightsCount, num_untextured_pointlights);
    if(mtl->_parUnTexPointLightsData)
      FXI->bindParamBlockBuffer(mtl->_parUnTexPointLightsData, pl_buffer);

    auto modcolor = context->RefModColor();
    FXI->BindParamVect4(mtl->_parModColor, modcolor*mtl->_baseColor);

  };
  return L;
}

////////////////////////////////////////////

void PBRMaterial::addBasicStateLambda(fxpipeline_ptr_t pipe){
  auto L = createBasicStateLambda(this);
  pipe->addStateLambda(L);
}
void PBRMaterial::addBasicStateLambda(){
  auto L = createBasicStateLambda(this);
  _state_lambdas.push_back(L);
}

////////////////////////////////////////////

void PBRMaterial::addLightingLambda(fxpipeline_ptr_t pipe){
  auto L = createLightingLambda(this);
  pipe->addStateLambda(L);
}
void PBRMaterial::addLightingLambda(){
  auto L = createLightingLambda(this);
  _state_lambdas.push_back(L);
}

////////////////////////////////////////////

static fxpipeline_ptr_t _createFxPipeline(const FxPipelinePermutation& permu,const PBRMaterial*mtl){

  fxpipeline_ptr_t pipeline;

  if(0 == strcmp(mtl->mMaterialName.c_str(),"Material.001")){
    printf( "yo\n");
  }

  bool is_picking = permu._is_picking;
  if(is_picking){
    //OrkBreak();
  }
  
  bool require_pbr_params = true;

  switch (mtl->_variant) {
    case 0: { // STANDARD VARIANT
      switch (permu._rendering_model) {
        //////////////////////////////////////////
        case "PICKING"_crcu: {
          OrkAssert(permu._stereo == false);
          require_pbr_params = false;
          if (permu._instanced and mtl->_tek_PIK_RI_IN) {
            pipeline                     = std::make_shared<FxPipeline>(permu);
            pipeline->_technique         = mtl->_tek_PIK_RI_IN;
            pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_Pick"_crcsh);
            //pipeline->bindParam(mtl->_paramM, "RCFD_M"_crcsh);
            //pipeline->bindParam(mtl->_paramMROT, "RCFD_Model_Rot"_crcsh);
            pipeline->bindParam(mtl->_parPickID, "RCID_PickID"_crcsh);
          }
          ////////////////
          else { // non-instanced
            if (permu._skinned and mtl->_tek_PIK_SK_NI) {
              //OrkBreak();
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_PIK_SK_NI;
              pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_Pick"_crcsh);
              pipeline->bindParam(mtl->_paramM, "RCFD_M"_crcsh);
              pipeline->bindParam(mtl->_paramMROT, "RCFD_Model_Rot"_crcsh);
              pipeline->bindParam(mtl->_parPickID, "RCID_PickID"_crcsh);
              //pipeline->_debugBreak = true;
            }
            else if (not permu._skinned and mtl->_tek_PIK_RI_NI) {
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_PIK_RI_NI;
              pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_Pick"_crcsh);
              pipeline->bindParam(mtl->_paramM, "RCFD_M"_crcsh);
              pipeline->bindParam(mtl->_paramMROT, "RCFD_Model_Rot"_crcsh);
              pipeline->bindParam(mtl->_parPickID, "RCID_PickID"_crcsh);
            }
            else{
              OrkAssert(false);
            }
            ////////////////
          }
          // OrkAssert(pipeline->_technique != nullptr);
          break;
        }
        //////////////////////////////////////////
        case "DEFERRED_PBR"_crcu: {

          if(is_picking){
            OrkAssert(false);
          }


          fxtechnique_constptr_t tek;
          ////////////////////////////////////////////////////////////////////////////////////////////
          auto common_lambda = [mtl](const RenderContextInstData& RCID, int ipass){
              auto _this       = (PBRMaterial*)mtl;
              auto RCFD        = RCID._RCFD;
              auto context     = RCFD->GetTarget();
              auto RSI         = context->RSI();
              const auto& CPD  = RCFD->topCPD();
              auto FXI         = context->FXI();
              auto modcolor = context->RefModColor();
              _this->_rasterstate.SetCullTest(ECullTest::PASS_FRONT);
              _this->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
              _this->_rasterstate.SetZWriteMask(true);
              _this->_rasterstate.SetRGBAWriteMask(true, true);
              RSI->BindRasterState(mtl->_rasterstate);
              FXI->BindParamVect4(_this->_parModColor, modcolor*_this->_baseColor);
          };
          printf( "OK1..\n");
          ////////////////////////////////////////////////////////////////////////////////////////////
          if (permu._stereo) {                                     // stereo
            if (permu._instanced) {                                // stereo-instanced
              tek = permu._skinned                  //
                                       ? mtl->_tek_GBU_CT_NM_SK_IN_ST //
                                       : mtl->_tek_GBU_CT_NM_RI_IN_ST;
            } else {                                             // stereo-non-instanced
              tek = permu._skinned                  //
                                       ? mtl->_tek_GBU_CT_NM_SK_NI_ST //
                                       : mtl->_tek_GBU_CT_NM_RI_NI_ST;
            }
            //////////////////////////////////
            if(tek){
              pipeline = std::make_shared<FxPipeline>(permu);
              pipeline->_technique = tek;
              pipeline->addStateLambda(common_lambda);
              pipeline->addStateLambda([mtl](const RenderContextInstData& RCID, int ipass) {
                auto _this       = (PBRMaterial*)mtl;
                auto RCFD        = RCID._RCFD;
                auto context     = RCFD->GetTarget();
                auto MTXI        = context->MTXI();
                auto FXI         = context->FXI();
                auto RSI         = context->RSI();
                const auto& CPD  = RCFD->topCPD();
                auto stereocams  = CPD._stereoCameraMatrices;
                auto worldmatrix = RCID.worldMatrix();
                auto modcolor = context->RefModColor();
                fmtx4 vrroot;
                auto vrrootprop = RCFD->getUserProperty("vrroot"_crc);
                if (auto as_mtx = vrrootprop.tryAs<fmtx4>()) {
                  vrroot = as_mtx.value();
                }
                FXI->BindParamMatrix(_this->_paramMVPL, stereocams->MVPL(vrroot*worldmatrix));
                FXI->BindParamMatrix(_this->_paramMVPR, stereocams->MVPR(vrroot*worldmatrix));
              });
            }
            else{
              OrkAssert(false);
            }
            //////////////////////////////////
          }
          ///// mono ///////////////////////////////////////////////////////////////////////////////////
          else {         
            printf( "OK2.. permu._instanced<%d> skinned<%d>\n", int(permu._instanced), int(permu._skinned) );


            if (permu._instanced) {    // mono-instanced
              tek = permu._skinned     //
                                       ? mtl->_tek_GBU_CT_NM_SK_IN_MO //
                                       : mtl->_tek_GBU_CT_NM_RI_IN_MO;
            } else {                   // mono-non-instanced
              tek = permu._skinned     //
                                       ? mtl->_tek_GBU_CT_NM_SK_NI_MO //
                                       : mtl->_tek_GBU_CT_NM_RI_NI_MO;
            }
            printf( "OK3.. mtl<%p> tek<%p>\n", mtl, tek );
            //////////////////////////////////
            if(tek){
              pipeline = std::make_shared<FxPipeline>(permu);
              pipeline->_technique = tek;
              pipeline->addStateLambda(common_lambda);
              pipeline->addStateLambda([mtl](const RenderContextInstData& RCID, int ipass) {
                auto _this       = (PBRMaterial*)mtl;
                auto RCFD        = RCID._RCFD;
                auto context     = RCFD->GetTarget();
                auto FXI         = context->FXI();
                auto MTXI        = context->MTXI();
                auto RSI         = context->RSI();
                const auto& CPD  = RCFD->topCPD();
                auto monocams    = CPD._cameraMatrices;
                auto worldmatrix = RCID.worldMatrix();
                auto eye_pos = monocams->_vmatrix.inverse().translation();
                FXI->BindParamVect3(mtl->_paramEyePostion, eye_pos);
                FXI->BindParamMatrix(_this->_paramM, worldmatrix);
                FXI->BindParamMatrix(_this->_paramMVP, monocams->MVPMONO(worldmatrix));
              });
            }
          }
          // OrkAssert(pipeline->_technique != nullptr);
          break;
        } // "DEFERRED_PB"_crcuR
        //////////////////////////////////////////
        case "FORWARD_UNLIT"_crcu:
        case 0:
          if(mtl->_tek_FWD_UNLIT_NI_MO){
            pipeline             = std::make_shared<FxPipeline>(permu);
            pipeline->_technique = mtl->_tek_FWD_UNLIT_NI_MO;
            //////////////////////////////////
            pipeline->addStateLambda([mtl](const RenderContextInstData& RCID, int ipass) {
              auto _this       = (PBRMaterial*)mtl;
              auto RCFD        = RCID._RCFD;
              auto context     = RCFD->GetTarget();
              auto FXI         = context->FXI();
              auto MTXI        = context->MTXI();
              auto RSI         = context->RSI();
              const auto& CPD  = RCFD->topCPD();
              auto monocams    = CPD._cameraMatrices;
              auto worldmatrix = RCID.worldMatrix();
              auto modcolor = context->RefModColor();
              FXI->BindParamVect4(_this->_parModColor, modcolor*_this->_baseColor);
              FXI->BindParamMatrix(_this->_paramMVP, monocams->MVPMONO(worldmatrix));
              _this->_rasterstate.SetCullTest(ECullTest::PASS_FRONT);
              _this->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
              _this->_rasterstate.SetZWriteMask(true);
              _this->_rasterstate.SetRGBAWriteMask(true, true);
              RSI->BindRasterState(_this->_rasterstate);

            });
          }
          break;
        case "FORWARD_PBR"_crcu: {
          ////////////////////////////////////////////////
          // setup forward lighting
          ////////////////////////////////////////////////


          ////////////////////////////////////////////////
          // set raster state
          ////////////////////////////////////////////////
          auto rsi_lambda = [mtl](const RenderContextInstData& RCID, int ipass) {
            auto _this      = (PBRMaterial*)mtl;
            auto RCFD       = RCID._RCFD;
            auto context    = RCFD->GetTarget();
            auto RSI        = context->RSI();
            _this->_rasterstate.SetCullTest(mtl->_doubleSided?ECullTest::OFF:ECullTest::PASS_FRONT);
            _this->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
            _this->_rasterstate.SetZWriteMask(true);
            _this->_rasterstate.SetRGBAWriteMask(true, true);
            RSI->BindRasterState(_this->_rasterstate);
          };

          if(permu._stereo){
            if (permu._instanced and not permu._skinned) {
              if(mtl->_tek_FWD_CT_NM_RI_IN_ST){
                pipeline          = std::make_shared<FxPipeline>(permu);
                pipeline->_technique         = mtl->_tek_FWD_CT_NM_RI_IN_ST;
                pipeline->bindParam(mtl->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
                pipeline->bindParam(mtl->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
                pipeline->addStateLambda(createBasicStateLambda(mtl));
                pipeline->addStateLambda(createLightingLambda(mtl));
                pipeline->addStateLambda(rsi_lambda);
              }
            }
            else if (not permu._instanced and not permu._skinned) {
              if(mtl->_tek_FWD_CT_NM_RI_NI_ST){
                pipeline          = std::make_shared<FxPipeline>(permu);
                pipeline->_technique         = mtl->_tek_FWD_CT_NM_RI_NI_ST;
                pipeline->bindParam(mtl->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
                pipeline->bindParam(mtl->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
                pipeline->addStateLambda(createBasicStateLambda(mtl));
                pipeline->addStateLambda(createLightingLambda(mtl));
                pipeline->addStateLambda(rsi_lambda);
              }
            }
            else if (not permu._instanced and permu._skinned) {
              if(mtl->_tek_FWD_CT_NM_SK_NI_ST){
                pipeline          = std::make_shared<FxPipeline>(permu);
                pipeline->_technique         = mtl->_tek_FWD_CT_NM_SK_NI_ST;
                pipeline->bindParam(mtl->_paramMVPL, "RCFD_Camera_MVP_Left"_crcsh);
                pipeline->bindParam(mtl->_paramMVPR, "RCFD_Camera_MVP_Right"_crcsh);
                pipeline->addStateLambda(createBasicStateLambda(mtl));
                pipeline->addStateLambda(createLightingLambda(mtl));
                pipeline->addStateLambda(rsi_lambda);
              }
            }
          }
          else{
            if (permu._instanced and not permu._skinned) {
              if(mtl->_tek_FWD_CT_NM_RI_IN_MO){
                pipeline          = std::make_shared<FxPipeline>(permu);
                pipeline->_technique         = mtl->_tek_FWD_CT_NM_RI_IN_MO;
                pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
                pipeline->addStateLambda(createBasicStateLambda(mtl));
                pipeline->addStateLambda(createLightingLambda(mtl));
                pipeline->addStateLambda(rsi_lambda);
              }
            }
            if (not permu._instanced and not permu._skinned) {
              if(mtl->_tek_FWD_CT_NM_RI_NI_MO){
                pipeline          = std::make_shared<FxPipeline>(permu);
                pipeline->_technique         = mtl->_tek_FWD_CT_NM_RI_NI_MO;
                pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
                pipeline->addStateLambda(createBasicStateLambda(mtl));
                pipeline->addStateLambda(createLightingLambda(mtl));
                pipeline->addStateLambda(rsi_lambda);
              }
            }            
          }


          // OrkAssert(pipeline->_technique != nullptr);
          break;
        }
        case "DEPTH_PREPASS"_crcu:
          if (not permu._instanced and not permu._skinned and not permu._stereo) {
            if(mtl->_tek_FWD_DEPTHPREPASS_NI_MO){
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_FWD_DEPTHPREPASS_NI_MO;
              pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
              pipeline->addStateLambda(createBasicStateLambda(mtl));
              pipeline->addStateLambda([mtl](const RenderContextInstData& RCID, int ipass) {
                auto _this   = (PBRMaterial*)mtl;
                auto RCFD    = RCID._RCFD;
                auto context = RCFD->GetTarget();
                auto FXI     = context->FXI();
                auto MTXI    = context->MTXI();
                auto RSI     = context->RSI();
                _this->_rasterstate.SetCullTest(mtl->_doubleSided?ECullTest::OFF:ECullTest::PASS_FRONT);
                _this->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
                _this->_rasterstate.SetZWriteMask(true);
                _this->_rasterstate.SetRGBAWriteMask(false, false);
                RSI->BindRasterState(_this->_rasterstate);
              });
            }
          }
          else if (permu._instanced and not permu._skinned and not permu._stereo) {
            if(mtl->_tek_FWD_DEPTHPREPASS_IN_MO){
              OrkAssert(false);
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_FWD_DEPTHPREPASS_IN_MO;
              pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_MVP_Mono"_crcsh);
              pipeline->addStateLambda(createBasicStateLambda(mtl));
              pipeline->addStateLambda([mtl](const RenderContextInstData& RCID, int ipass) {
                auto _this   = (PBRMaterial*)mtl;
                auto RCFD    = RCID._RCFD;
                auto context = RCFD->GetTarget();
                auto FXI     = context->FXI();
                auto MTXI    = context->MTXI();
                auto RSI     = context->RSI();
                _this->_rasterstate.SetCullTest(mtl->_doubleSided?ECullTest::OFF:ECullTest::PASS_FRONT);
                _this->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
                _this->_rasterstate.SetZWriteMask(true);
                _this->_rasterstate.SetRGBAWriteMask(false, false);
                RSI->BindRasterState(_this->_rasterstate);
              });
            }
          }
          // OrkAssert(pipeline->_technique != nullptr);
          break;
        default:
          OrkAssert(false);
          break;
          //////////////////////////////////////////
      } // switch (cfg._rendering_model) {
      break;
    } // case 0: // STANDARD VARIANT
    //////////////////////////////////////////
    case "skybox.forward"_crcu: { // FORWARD SKYBOX VARIANT
      auto basic_lambda  = createBasicStateLambda(mtl);
      auto skybox_lambda = [mtl, basic_lambda](const RenderContextInstData& RCID, int ipass) {
        auto _this   = (PBRMaterial*)mtl;
        auto RCFD    = RCID._RCFD;
        auto context = RCFD->GetTarget();
        auto FXI     = context->FXI();
        auto MTXI    = context->MTXI();
        auto RSI     = context->RSI();
        auto pbrcommon        = RCFD->_pbrcommon;
        auto envtex  = pbrcommon->envSpecularTexture();

        FXI->BindParamCTex(_this->_parMapSpecularEnv, envtex.get() );

        basic_lambda(RCID, ipass);
        _this->_rasterstate.SetCullTest(ECullTest::OFF);
        _this->_rasterstate.SetZWriteMask(false);
        _this->_rasterstate.SetDepthTest(EDepthTest::LEQUALS);
        _this->_rasterstate.SetRGBAWriteMask(true, true);
        RSI->BindRasterState(_this->_rasterstate);
      };
      //////////////////////////////////////////////////////////
      OrkAssert(permu._instanced==false);
      OrkAssert(permu._skinned==false);
      //////////////////////////////////////////////////////////
      if(permu._stereo and mtl->_tek_FWD_SKYBOX_ST){
        auto pipeline_stereo        = std::make_shared<FxPipeline>(permu);
        pipeline_stereo->_technique = mtl->_tek_FWD_SKYBOX_ST;
        pipeline_stereo->bindParam(mtl->_paramIVPL, "RCFD_Camera_IVP_Left"_crcsh);
        pipeline_stereo->bindParam(mtl->_paramIVPR, "RCFD_Camera_IVP_Right"_crcsh);
        pipeline_stereo->addStateLambda(skybox_lambda);
        pipeline_stereo->_material = (GfxMaterial*)mtl;
        pipeline = pipeline_stereo;
      }
      else if(mtl->_tek_FWD_SKYBOX_MO){
        auto pipeline_stereo        = std::make_shared<FxPipeline>(permu);
        pipeline_stereo->_technique = mtl->_tek_FWD_SKYBOX_MO;
        pipeline_stereo->bindParam(mtl->_paramIVP, "RCFD_Camera_IVP_Mono"_crcsh);
        pipeline_stereo->addStateLambda(skybox_lambda);
        pipeline_stereo->_material = (GfxMaterial*)mtl;
        pipeline = pipeline_stereo;
      }
      //////////////////////////////////////////////////////////
      break;
    }
    //////////////////////////////////////////
    case "normalviz"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "vertexcolor"_crcu: {
      auto no_cull_stateblock = [mtl](const RenderContextInstData& RCID, int ipass) {
        auto _this   = (PBRMaterial*)mtl;
        auto RCFD    = RCID._RCFD;
        auto context = RCFD->GetTarget();
        auto FXI     = context->FXI();
        auto MTXI    = context->MTXI();
        auto RSI     = context->RSI();
        _this->_rasterstate.SetCullTest(ECullTest::OFF);
        _this->_rasterstate.SetDepthTest(EDepthTest::OFF);
        _this->_rasterstate.SetZWriteMask(true);
        _this->_rasterstate.SetRGBAWriteMask(true, false);
        RSI->BindRasterState(_this->_rasterstate);
      };
      switch (permu._rendering_model) {
        case "FORWARD_PBR"_crcu: {
          if (not permu._instanced and not permu._skinned and not permu._stereo) {
            if(mtl->_tek_FWD_CV_EMI_RI_NI_MO){
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_FWD_CV_EMI_RI_NI_MO;
              pipeline->bindParam(mtl->_paramMVP,"RCFD_Camera_MVP_Mono"_crcsh);
              pipeline->addStateLambda(createBasicStateLambda(mtl));
              pipeline->addStateLambda(no_cull_stateblock);
              OrkAssert(pipeline->_technique != nullptr);
            }
          }
          break;
        }
        case "DEFERRED_PBR"_crcu: {
          if (not permu._instanced and not permu._skinned and not permu._stereo) {
            if(mtl->_tek_GBU_CV_EMI_RI_NI_MO){
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_GBU_CV_EMI_RI_NI_MO;
              pipeline->bindParam(mtl->_paramMVP,"RCFD_Camera_MVP_Mono"_crcsh);
              pipeline->addStateLambda(createBasicStateLambda(mtl));
              pipeline->addStateLambda(no_cull_stateblock);
              OrkAssert(pipeline->_technique != nullptr);
            }
          }
          break;
        }
        case "PICKING"_crcu: {
          if (not permu._instanced and not permu._skinned and not permu._stereo) {
            if(mtl->_tek_PIK_RI_NI){
              pipeline                     = std::make_shared<FxPipeline>(permu);
              pipeline->_technique         = mtl->_tek_PIK_RI_NI;
              pipeline->bindParam(mtl->_paramMVP, "RCFD_Camera_Pick"_crcsh);
              OrkAssert(pipeline->_technique != nullptr);
            }
          }
          break;
        }
        default:
          break;
      }
      break;
    }
    //////////////////////////////////////////
    case "font"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    case "font-instanced"_crcu:
      OrkAssert(false);
      break;
    //////////////////////////////////////////
    default:
      OrkAssert(false);
      break;
  }

  if (pipeline and pipeline->_technique) {

    pipeline->bindParam(mtl->_paramMROT,"RCFD_Model_Rot"_crcsh);

    if( require_pbr_params ){
      pipeline->bindParam(mtl->_paramMapColor,mtl->_texColor);
      pipeline->bindParam(mtl->_paramMapNormal,mtl->_texNormal);
      pipeline->bindParam(mtl->_paramMapMtlRuf,mtl->_texMtlRuf);
      pipeline->bindParam(mtl->_paramMapEmissive,mtl->_texEmissive);

      pipeline->bindParam(mtl->_parMetallicFactor,mtl->_metallicFactor);
      pipeline->bindParam(mtl->_parRoughnessFactor,mtl->_roughnessFactor);
    }

    pipeline->_parInstanceMatrixMap = mtl->_paramInstanceMatrixMap;
    pipeline->_parInstanceIdMap     = mtl->_paramInstanceIdMap;
    pipeline->_parInstanceColorMap  = mtl->_paramInstanceColorMap;
    pipeline->_material             = (GfxMaterial*)mtl;


    for( auto l : mtl->_state_lambdas ){
      pipeline->addStateLambda(l);
    }
    for( auto item : mtl->_bound_params ){
      pipeline->bindParam(item.first, item.second);
    }


  }
  else{
    printf("No PIPELINE for mtl<%s> variant<%08x>\n", mtl->mMaterialName.c_str(), mtl->_variant );
    printf( "permu-renderingmodel<%08x>\n", permu._rendering_model );
    OrkAssert(false);
  }

  return pipeline;
}

///////////////////////////////////////////////////////////////////////////////

fxpipelinecache_constptr_t PBRMaterial::_doFxPipelineCache(fxpipelinepermutation_set_constptr_t perms) const { // final
  return _getpbrcache()->getCache(this);
}

/////////////////////////////////////////////////////////////////////////

PbrMatrixBlockApplicator* PbrMatrixBlockApplicator::getApplicator() {
  static PbrMatrixBlockApplicator* _gapplicator = new PbrMatrixBlockApplicator;
  return _gapplicator;
}

void PBRMaterial::describeX(class_t* c) {

  /////////////////////////////////////////////////////////////////

  chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx) -> ork::lev2::material_ptr_t {
    auto targ             = ctx._varmap->typedValueForKey<Context*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap->typedValueForKey<embtexmap_t>("embtexmap").value();

    for( auto item : embtexmap ){
      logchan_pbr->log("embtex<%s>", item.first.c_str() );
    }

    int istring = 0;

    ctx._inputStream->GetItem(istring);
    auto materialname = ctx._reader.GetString(istring);

    ctx._inputStream->GetItem(istring);
    auto texbasename = ctx._reader.GetString(istring);
    auto mtl         = std::make_shared<PBRMaterial>();
    mtl->SetName(AddPooledString(materialname));
    logchan_pbr->log("read.xgm: materialName<%s>", materialname);
    ctx._inputStream->GetItem(istring);
    auto begintextures = ctx._reader.GetString(istring);
    OrkAssert(0 == strcmp(begintextures, "begintextures"));
    bool done = false;
    while (false == done) {
      ctx._inputStream->GetItem(istring);
      auto token = ctx._reader.GetString(istring);
      if (0 == strcmp(token, "endtextures"))
        done = true;
      else {
        ctx._inputStream->GetItem(istring);
        auto texname = ctx._reader.GetString(istring);
        logchan_pbr->log("read.xgm: find tex channel<%s> texname<%s> .. ", token, texname);
        auto itt = embtexmap.find(texname);
        OrkAssert(itt != embtexmap.end());
        auto embtex = itt->second;
        logchan_pbr->log("read.xgm: embtex<%p> data<%p> len<%zu>", embtex, embtex->_srcdata, embtex->_srcdatalen);
        auto tex = std::make_shared<lev2::Texture>();
        // crashes here...
        auto datablock = std::make_shared<DataBlock>(embtex->_srcdata, embtex->_srcdatalen);
        bool ok        = txi->LoadTexture(tex, datablock);
        OrkAssert(ok);
        logchan_pbr->log(" embtex<%p> datablock<%p> len<%zu>", embtex, datablock.get(), datablock->length());
        logchan_pbr->log(" token<%s>", token );
        if (0 == strcmp(token, "colormap")) {
          mtl->_texColor = tex;
          mtl->_colorMapName = texname;
        }
        if (0 == strcmp(token, "normalmap")) {
          mtl->_texNormal = tex;
          mtl->_normalMapName = texname;
        }
        if (0 == strcmp(token, "mtlrufmap")) {
          mtl->_texMtlRuf = tex;
          mtl->_mtlRufMapName = texname;
        }
        if (0 == strcmp(token, "emissivemap")) {
          mtl->_texEmissive = tex;
          mtl->_emissiveMapName = texname;
        }
      }
    }
    ctx._inputStream->GetItem<float>(mtl->_metallicFactor);
    ctx._inputStream->GetItem<float>(mtl->_roughnessFactor);
    ctx._inputStream->GetItem<fvec4>(mtl->_baseColor);
    //logchan_pbr->log("read.xgm: basecolor<%g %g %g>", mtl->_baseColor.x,mtl->_baseColor.y,mtl->_baseColor.z);

    if (auto try_ov = ctx._varmap->typedValueForKey<std::string>("override.shader.gbuf")) {
      const auto& ov_val = try_ov.value();
      if (ov_val == "normalviz") {
        mtl->_variant = "normalviz"_crcu;
      }
    }

    return mtl;
  };

  /////////////////////////////////////////////////////////////////

  chunkfile::materialwriter_t writer = [](chunkfile::XgmMaterialWriterContext& ctx) {
    auto pbrmtl = std::static_pointer_cast<const PBRMaterial>(ctx._material);

    int istring = ctx._writer.stringIndex(pbrmtl->mMaterialName.c_str());
    ctx._outputStream->AddItem(istring);

    istring = ctx._writer.stringIndex(pbrmtl->_textureBaseName.c_str());
    ctx._outputStream->AddItem(istring);

    auto dotex = [&](std::string channelname, std::string texname) {
      //logchan_pbr->log("write.xgm: tex channel<%s> texname<%s>", channelname.c_str(), texname.c_str());
      if (texname.length()) {
        istring = ctx._writer.stringIndex(channelname.c_str());
        ctx._outputStream->AddItem(istring);
        istring = ctx._writer.stringIndex(texname.c_str());
        ctx._outputStream->AddItem(istring);
      }
    };
    istring = ctx._writer.stringIndex("begintextures");
    ctx._outputStream->AddItem(istring);
    dotex("colormap", pbrmtl->_colorMapName);
    dotex("normalmap", pbrmtl->_normalMapName);
    dotex("amboccmap", pbrmtl->_amboccMapName);
    dotex("emissivemap", pbrmtl->_emissiveMapName);
    dotex("mtlrufmap", pbrmtl->_mtlRufMapName);
    istring = ctx._writer.stringIndex("endtextures");
    ctx._outputStream->AddItem(istring);

    ctx._outputStream->AddItem<float>(pbrmtl->_metallicFactor);
    ctx._outputStream->AddItem<float>(pbrmtl->_roughnessFactor);
    ctx._outputStream->AddItem<fvec4>(pbrmtl->_baseColor);
    //logchan_pbr->log("write.xgm: _metallicFactor<%g>", pbrmtl->_metallicFactor);
    //logchan_pbr->log("write.xgm: _roughnessFactor<%g>", pbrmtl->_roughnessFactor);
    //logchan_pbr->log(
      //  "write.xgm: _baseColor<%g %g %g %g>", //
        //pbrmtl->_baseColor.x,                 //
        //pbrmtl->_baseColor.y,                 //
        //pbrmtl->_baseColor.z,                 //
        //pbrmtl->_baseColor.w);
  };

  /////////////////////////////////////////////////////////////////

  c->annotate("xgm.writer", writer);
  c->annotate("xgm.reader", reader);
}

////////////////////////////////////////////

void PBRMaterial::gpuInit(Context* targ) /*final*/ {

  printf( "PBRMaterial::gpuInit<%p> _initialTarget<%p> targ<%p>\n", this, _initialTarget, targ );

  if (_initialTarget)
    return;

  _initialTarget = targ;
  auto fxi       = targ->FXI();

  auto loadreq = std::make_shared<asset::LoadRequest>();

  //printf( "PBRMaterial::gpuInit<%p> _shaderpath<%s>\n", this, _shaderpath.c_str() );
  loadreq->_asset_path = _shaderpath;

  _as_freestyle = std::make_shared<FreestyleMaterial>();
  _as_freestyle->gpuInit(targ,_shaderpath);
  _asset_shader = _as_freestyle->_shaderasset;
  _shader       = _as_freestyle->_shader;
  

  // specials

  _tek_GBU_DB_NM_NI_MO = fxi->technique(_shader, "GBU_DB_NM_NI_MO"s+_shader_suffix);

  _tek_GBU_CF_IN_MO = fxi->technique(_shader, "GBU_CF_IN_MO"s+_shader_suffix);
  _tek_GBU_CF_NI_MO = fxi->technique(_shader, "GBU_CF_NI_MO"s+_shader_suffix);

  _tek_PIK_RI_IN = fxi->technique(_shader, "PIK_RI_IN"s+_shader_suffix);
  _tek_PIK_RI_NI = fxi->technique(_shader, "PIK_RI_NI"s+_shader_suffix);
  _tek_PIK_SK_NI = fxi->technique(_shader, "PIK_SK_NI"s+_shader_suffix);

  // forwards

  _tek_FWD_UNLIT_NI_MO = fxi->technique(_shader, "FWD_UNLIT_NI_MO"s+_shader_suffix);

  _tek_FWD_SKYBOX_MO          = fxi->technique(_shader, "FWD_SKYBOX_MO"s+_shader_suffix);
  _tek_FWD_SKYBOX_ST          = fxi->technique(_shader, "FWD_SKYBOX_ST"s+_shader_suffix);
  _tek_FWD_CT_NM_RI_NI_MO     = fxi->technique(_shader, "FWD_CT_NM_RI_NI_MO"s+_shader_suffix);
  _tek_FWD_CT_NM_RI_IN_MO     = fxi->technique(_shader, "FWD_CT_NM_RI_IN_MO"s+_shader_suffix);
  _tek_FWD_CT_NM_RI_NI_ST     = fxi->technique(_shader, "FWD_CT_NM_RI_NI_ST"s+_shader_suffix);
  _tek_FWD_CT_NM_RI_IN_ST     = fxi->technique(_shader, "FWD_CT_NM_RI_IN_ST"s+_shader_suffix);
  _tek_FWD_DEPTHPREPASS_IN_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_IN_MO"s+_shader_suffix);

  _tek_FWD_DEPTHPREPASS_NI_MO = fxi->technique(_shader, "FWD_DEPTHPREPASS_RI_NI_MO"s+_shader_suffix);

  _tek_FWD_CV_EMI_RI_NI_MO = fxi->technique(_shader, "FWD_CV_EMI_RI_NI_MO"s+_shader_suffix);

  _tek_FWD_CT_NM_SK_NI_ST = fxi->technique(_shader, "FWD_CT_NM_SK_NI_ST"s+_shader_suffix);

  // deferreds

  _tek_GBU_CM_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CM_NM_RI_NI_MO"s+_shader_suffix);
  _tek_GBU_CM_NM_SK_NI_MO = fxi->technique(_shader, "GBU_CM_NM_SK_NI_MO"s+_shader_suffix);
  _tek_GBU_CM_NM_RI_NI_ST = fxi->technique(_shader, "GBU_CM_NM_RI_NI_ST"s+_shader_suffix);

  _tek_GBU_CT_NM_RI_IN_MO = fxi->technique(_shader, "GBU_CT_NM_RI_IN_MO"s+_shader_suffix);
  _tek_GBU_CT_NM_RI_IN_ST = fxi->technique(_shader, "GBU_CT_NM_RI_IN_ST"s+_shader_suffix);
  _tek_GBU_CT_NM_RI_NI_ST = fxi->technique(_shader, "GBU_CT_NM_RI_NI_ST"s+_shader_suffix);
  _tek_GBU_CT_NM_RI_NI_MO = fxi->technique(_shader, "GBU_CT_NM_RI_NI_MO"s+_shader_suffix);

  _tek_GBU_CT_NM_SK_IN_MO = fxi->technique(_shader, "GBU_CT_NM_SK_IN_MO"s+_shader_suffix);

  _tek_GBU_CT_NM_SK_NI_MO = fxi->technique(_shader, "GBU_CT_NM_SK_NI_MO"s+_shader_suffix);

  _tek_GBU_CT_NV_RI_NI_MO = fxi->technique(_shader, "GBU_CT_NV_RI_NI_MO"s+_shader_suffix);

  _tek_GBU_CV_EMI_RI_NI_MO = fxi->technique(_shader, "GBU_CV_EMI_RI_NI_MO"s+_shader_suffix);


  printf( "_tek_GBU_CT_NM_RI_NI_MO<%p>\n", _tek_GBU_CT_NM_RI_NI_MO );
  printf( "_tek_GBU_CM_NM_RI_NI_MO<%p>\n", _tek_GBU_CM_NM_RI_NI_MO );
  // OrkAssert(_tek_GBU_CT_NM_RI_NI_ST);
  // OrkAssert(_tek_GBU_CT_NM_RI_IN_ST);
  // OrkAssert(_tek_GBU_CT_NM_RI_IN_MO);
  // OrkAssert(_tek_GBU_CT_NM_RI_NI_MO);
  // OrkAssert(_tek_FWD_CT_NM_RI_NI_MO);
  // OrkAssert(_tek_FWD_CT_NM_RI_IN_MO);

  // parameters

  _paramM                 = fxi->parameter(_shader, "m");
  _paramV                 = fxi->parameter(_shader, "v");
  _paramVP                = fxi->parameter(_shader, "vp");
  _paramVL               = fxi->parameter(_shader, "v_l");
  _paramVR               = fxi->parameter(_shader, "v_r");
  _paramVPL               = fxi->parameter(_shader, "vp_l");
  _paramVPR               = fxi->parameter(_shader, "vp_r");
  _paramIVPL              = fxi->parameter(_shader, "inv_vp_l");
  _paramIVPR              = fxi->parameter(_shader, "inv_vp_r");
  _paramIVP               = fxi->parameter(_shader, "inv_vp");
  _paramMVP               = fxi->parameter(_shader, "mvp");
  _paramMVPL              = fxi->parameter(_shader, "mvp_l");
  _paramMVPR              = fxi->parameter(_shader, "mvp_r");
  _paramMV                = fxi->parameter(_shader, "mv");
  _paramMROT              = fxi->parameter(_shader, "mrot");
  _paramMapColor          = fxi->parameter(_shader, "ColorMap");
  _paramMapNormal         = fxi->parameter(_shader, "NormalMap");
  _paramMapMtlRuf         = fxi->parameter(_shader, "MtlRufMap");
  _paramMapEmissive       = fxi->parameter(_shader, "EmissiveMap");
  _parMapAmbOcc           = fxi->parameter(_shader, "AmbOccMap");
  _parMapLightMap         = fxi->parameter(_shader, "LightMap");
  _parInvViewSize         = fxi->parameter(_shader, "InvViewportSize");
  _parMetallicFactor      = fxi->parameter(_shader, "MetallicFactor");
  _parRoughnessFactor     = fxi->parameter(_shader, "RoughnessFactor");
  _parModColor            = fxi->parameter(_shader, "ModColor");
  _parPickID              = fxi->parameter(_shader, "obj_pickID");
  _paramInstanceMatrixMap = fxi->parameter(_shader, "InstanceMatrices");
  _paramInstanceIdMap     = fxi->parameter(_shader, "InstanceIds");
  _paramInstanceColorMap  = fxi->parameter(_shader, "InstanceColors");

  _parBoneBlock = fxi->parameterBlock(_shader, "ub_vtx_boneblock");

  // fwd

  _paramEyePostion    = fxi->parameter(_shader, "EyePostion");
  _paramEyePostionL    = fxi->parameter(_shader, "EyePostionL");
  _paramEyePostionR    = fxi->parameter(_shader, "EyePostionR");
  _paramAmbientLevel  = fxi->parameter(_shader, "AmbientLevel");
  _paramDiffuseLevel  = fxi->parameter(_shader, "DiffuseLevel");
  _paramSpecularLevel = fxi->parameter(_shader, "SpecularLevel");
  _paramSkyboxLevel   = fxi->parameter(_shader, "SkyboxLevel");

  _parSpecularMipBias  = fxi->parameter(_shader, "SpecularMipBias");

  _parMapSpecularEnv      = fxi->parameter(_shader, "MapSpecularEnv");
  _parMapDiffuseEnv       = fxi->parameter(_shader, "MapDiffuseEnv");
  _parMapBrdfIntegration  = fxi->parameter(_shader, "MapBrdfIntegration");
  _parEnvironmentMipBias  = fxi->parameter(_shader, "EnvironmentMipBias");
  _parEnvironmentMipScale = fxi->parameter(_shader, "EnvironmentMipScale");
  _parDepthFogDistance    = fxi->parameter(_shader, "DepthFogDistance");
  _parDepthFogPower       = fxi->parameter(_shader, "DepthFogPower");

  _parUnTexPointLightsCount = fxi->parameter(_shader, "point_light_count");
  _parUnTexPointLightsData  = fxi->parameterBlock(_shader, "ub_frg_fwd_lighting");


  //

  OrkAssert(_paramMapNormal != nullptr);
  OrkAssert(_parBoneBlock != nullptr);

  printf( "_texColor<%p>\n", _texColor.get() );
  printf( "_texNormal<%p>\n", _texNormal.get() );
  printf( "_texMtlRuf<%p>\n", _texMtlRuf.get() );

  if (_texColor == nullptr) {
    auto loadreq = std::make_shared<asset::LoadRequest>();
    loadreq->_asset_path = "src://effect_textures/white";
    _asset_texcolor = asset::AssetManager<lev2::TextureAsset>::load(loadreq);
    _texColor       = _asset_texcolor->GetTexture();
    // logchan_pbr->log("substituted white for non-existant color texture");
    OrkAssert(_texColor != nullptr);
  }
  if (_texNormal == nullptr) {
    static auto defntex = targ->TXI()->createColorTexture(fvec4(0.5,0.5,1,1),8,8);
    _texNormal       = defntex;
    OrkAssert(_texNormal != nullptr);
  }
  if (_texMtlRuf == nullptr) {

    static auto defmrtex = targ->TXI()->createColorTexture(fvec4(1,1,0,1),8,8);
    _texMtlRuf    = defmrtex;

    if(_metallicFactor!=0.0f){
      static auto metallictex = targ->TXI()->createColorTexture(fvec4(1,0,1,1),8,8);
      _texMtlRuf    = metallictex;
    }

    OrkAssert(_texMtlRuf != nullptr);
  }
  if (_texEmissive == nullptr) {
    static auto defemitex = targ->TXI()->createColorTexture(fvec4(0,0,0,0),8,8);
    _texEmissive    = defemitex;
  }
}

void PBRMaterial::forceEmissive() {
    auto loadreq = std::make_shared<asset::LoadRequest>();
    loadreq->_asset_path = "src://effect_textures/black";
  // to force emissive set normal map to black
  // shader will interpret as emissive
  _asset_texnormal = asset::AssetManager<lev2::TextureAsset>::load(loadreq);
  _texNormal       = _asset_texnormal->GetTexture();
  OrkAssert(_texNormal != nullptr);
}

////////////////////////////////////////////

int PBRMaterial::BeginBlock(Context* context, const RenderContextInstData& RCID) {
  auto fxi   = context->FXI();
  auto fxcache = RCID._pipeline_cache;
  OrkAssert(fxcache);
  auto pipelineance = fxcache->findPipeline(RCID);
  OrkAssert(pipelineance);
  auto tek = pipelineance->_technique;
  OrkAssert(tek);

  int numpasses = fxi->BeginBlock(tek, RCID);
  OrkAssert(numpasses == 1);
  return numpasses;
}

////////////////////////////////////////////

void PBRMaterial::EndBlock(Context* context) {
  auto fxi = context->FXI();
  fxi->EndBlock();
}

////////////////////////////////////////////

void PBRMaterial::gpuUpdate(Context* context) {
  GfxMaterial::gpuUpdate(context);
  // auto fxi    = context->FXI();
}

////////////////////////////////////////////

bool PBRMaterial::BeginPass(Context* targ, int iPass) {
  auto fxi = targ->FXI();
  auto rsi = targ->RSI();
  fxi->BindPass(0);
  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
}

void PBRMaterial::UpdateMVPMatrix(Context* context) {
  auto fxi                           = context->FXI();
  auto rsi                           = context->RSI();
  auto mtxi                          = context->MTXI();
  const RenderContextInstData* RCID  = context->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = context->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
  } else {
    auto mcams        = CPD._cameraMatrices;
    const auto& world = mtxi->RefMMatrix();
    auto MVP          = fmtx4::multiply_ltor(world, mcams->_vmatrix, mcams->_pmatrix);
    fxi->BindParamMatrix(_paramV, mcams->_vmatrix);
    fxi->BindParamMatrix(_paramMVP, MVP);
  }
}

void PBRMaterial::UpdateMMatrix(Context* context) {
  auto fxi          = context->FXI();
  auto mtxi         = context->MTXI();
  const auto& world = mtxi->RefMMatrix();
  fxi->BindParamMatrix(_paramM, world);
}

////////////////////////////////////////////

void PBRMaterial::EndPass(Context* targ) {
  targ->FXI()->EndPass();
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::BindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////
  auto mtxblockitem = dynamic_cast<MaterialInstItemMatrixBlock*>(pitem);

  if (mtxblockitem) {
    // if (hBoneMatrices->GetPlatformHandle()) {
    auto applicator = PbrMatrixBlockApplicator::getApplicator();
    OrkAssert(applicator != 0);
    applicator->_pbrmaterial = this;
    applicator->_matrixblock = mtxblockitem;
    mtxblockitem->SetApplicator(applicator);
    //}
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::UnBindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////

  auto mtxblockitem = dynamic_cast<MaterialInstItemMatrixBlock*>(pitem);

  if (mtxblockitem) {
    // if (hBoneMatrices->GetPlatformHandle()) {
    auto applicator = static_cast<PbrMatrixBlockApplicator*>(mtxblockitem->mApplicator);
    if (applicator) {
      applicator->_pbrmaterial = nullptr;
      applicator->_matrixblock = nullptr;
    }
    //}
    return;
  }
}

///////////////////////////////////////////////////////////////////////////////

void PbrMatrixBlockApplicator::ApplyToTarget(Context* context) // virtual
{
  auto fxi                           = context->FXI();
  auto mtxi                          = context->MTXI();
  const RenderContextInstData* RCID  = context->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = context->topRenderContextFrameData();
  const auto& CPD                    = RCFD->topCPD();
  const auto& world                  = mtxi->RefMMatrix();
  const auto& drect                  = CPD.GetDstRect();
  const auto& mrect                  = CPD.GetMrtRect();
  FxShader* shader                   = _pbrmaterial->_shader;
  size_t inumbones                   = _matrixblock->GetNumMatrices();
  const fmtx4* Matrices              = _matrixblock->GetMatrices();
  size_t fmtx4_stride    = sizeof(fmtx4);

  auto bones_buffer = PBRMaterial::boneDataBuffer(context);
  auto bones_mapped = fxi->mapParamBuffer(bones_buffer, 0, inumbones*sizeof(fmtx4));

  //printf( "inumbones<%d>\n", inumbones );

  for (int i = 0; i < inumbones; i++) {
    bones_mapped->ref<fmtx4>(fmtx4_stride*i) = Matrices[i];
    //printf( "I<%d>: ", i );
    //Matrices[i].dump("bonemtx");
  }

  bones_mapped->unmap();

  if(_pbrmaterial->_parBoneBlock){
    fxi->bindParamBlockBuffer(_pbrmaterial->_parBoneBlock, bones_buffer);
  }
}

////////////////////////////////////////////

void PBRMaterial::Update() {
}

////////////////////////////////////////////

void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}

PBRMaterial::PBRMaterial(Context* targ)
    : PBRMaterial() {
  gpuInit(targ);
}

PBRMaterial::PBRMaterial()
    : _baseColor(1, 1, 1) {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(Blending::OFF);
  _rasterstate.SetDepthTest(EDepthTest::LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECullTest::PASS_FRONT);
  miNumPasses = 1;
  _shaderpath = "orkshader://pbr";
  printf( "new PBRMaterial<%p>\n", this );
}

////////////////////////////////////////////

PBRMaterial::~PBRMaterial() {
}

////////////////////////////////////////////

pbrmaterial_ptr_t PBRMaterial::clone() const {
  auto copy = std::make_shared<PBRMaterial>();
  copy->_asset_shader = _asset_shader;
  copy->_asset_texcolor = _asset_texcolor;
  copy->_asset_texnormal = _asset_texnormal;
  copy->_asset_mtlruf = _asset_mtlruf;
  copy->_asset_emissive = _asset_emissive;

  copy->_texColor = _texColor;
  copy->_texNormal = _texNormal;
  copy->_texMtlRuf = _texMtlRuf;
  copy->_texEmissive = _texEmissive;
  copy->_textureBaseName = _textureBaseName;

  copy->_colorMapName = _colorMapName;
  copy->_normalMapName = _normalMapName;
  copy->_mtlRufMapName = _mtlRufMapName;
  copy->_amboccMapName = _amboccMapName;
  copy->_emissiveMapName = _emissiveMapName;
  copy->_shaderpath = _shaderpath;

  copy->_metallicFactor = _metallicFactor;
  copy->_roughnessFactor = _roughnessFactor;
  copy->_baseColor = _baseColor;
  copy->_textureBaseName = _textureBaseName;
  
  copy->_variant = _variant;
  copy->mMaterialName = mMaterialName;
  copy->_varmap = _varmap;

  // TODO - flyweight clones

  if(_initialTarget){
    copy->gpuInit(_initialTarget);
  }
  return copy;
}

////////////////////////////////////////////

} // namespace ork::lev2
