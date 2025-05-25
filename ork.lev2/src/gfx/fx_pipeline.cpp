////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/util/logger.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

namespace ork::lev2 {
static logchannel_ptr_t logchan_fxcache = logger()->createChannel("fxcache",fvec3(0.7,0.7,0.5),false);
///////////////////////////////////////////////////////////////////////////////
uint64_t FxPipelinePermutation::genIndex() const {
  uint64_t index = 0;
  index += (uint64_t(_stereo) << 1);
  index += (uint64_t(_instanced) << 2);
  index += (uint64_t(_skinned) << 3);
  index += (uint64_t(_is_picking) << 4);
  index += (uint64_t(_has_vtxcolors) << 5);
  index += (uint64_t(_rendering_model) << 16);

  auto tekovr = uint64_t((const void*)_forced_technique);
  index += tekovr;
  return index;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipelinePermutation::dump() const {
  std::string rmodel = FormatString("0x%zx", uint64_t(_rendering_model));

  switch(_rendering_model){
    case "DEFERRED_PBR"_crcu:
      rmodel = "DEFERRED_PBR";
      break;
    case "DeferredPBR"_crcu:
      rmodel = "DeferredPBR(Did you mean DEFERRED_PBR?)";
      break;
    case "FORWARD_PBR"_crcu:
      rmodel = "FORWARD_PBR";
      break;
    default:
      break;
  }
  printf(
      "configdump: rendering_model<0x%zx> stereo<%d> instanced<%d> skinned<%d> picking<%d>\n",
      size_t(_rendering_model),
      int(_stereo),
      int(_instanced),
      int(_skinned),
      int(_is_picking));
}
///////////////////////////////////////////////////////////////////////////////
void FxPipelinePermutationSet::add(fxpipelinepermutation_constptr_t perm){
  __permutations.insert(perm);
}
/////////////////////////////////////////////////////////////////////////
FxPipeline::FxPipeline(const FxPipelinePermutation& config)
    : __permutation(config) {
    _vars = std::make_shared<varmap::VarMap>();
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::bindParam(fxparam_constptr_t p, varval_t v){
  OrkAssert(p!=nullptr);
  _params[p] = v;
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall) {
  if(_debugBreak){
      OrkBreak();
  }
  int inumpasses = beginBlock(RCID);
  if(_debugPrint){
    printf( "FxPipeline<%p:%s> wrappedDrawCall inumpasses<%d>\n", this, _debugName.c_str(), inumpasses );
  }
  drawcall();
  endBlock(RCID);
}
///////////////////////////////////////////////////////////////////////////////
int FxPipeline::beginBlock(const RenderContextInstData& RCID) {
  auto context    = RCID.rcfd()->GetTarget();
  auto FXI        = context->FXI();
  int rval = FXI->BeginBlock(_technique, RCID);

  if( _debugBreak ){
    OrkBreak();
  }

  FXI->_debugDrawCall = _debugPrint;
  //bool OK = FXI->BindPass(ipass);
  FXI->_debugDrawCall = false;
  //if (not OK)
    //return OK;

  ///////////////////////////////
  // run state lambdas
  ///////////////////////////////

  if(_debugPrint){
    printf( "FxPipeline<%p:%s>::beginBlock num_statelambdas<%zu>\n", this, _debugName.c_str(), _statelambdas.size() );
  }

  for( auto& item: _statelambdas ){
    item(RCID);
  }

  ///////////////////////////////
  // run individual state items
  ///////////////////////////////

  if(_debugPrint){
    printf( "FxPipeline<%p:%s>::beginBlock num_params<%zu>\n", this, _debugName.c_str(), _params.size() );
  }

  for (auto item : _params) {
    fxparam_constptr_t param = item.first;
    const auto& val          = item.second;
    _set_typed_param(RCID,param,val);
  }

  ///////////////////////////////
  // apply raster state
  ///////////////////////////////
  if(_rasterstate){
    FXI->applyRasterState(*_rasterstate);
  }

  ///////////////////////////////

  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::_set_typed_param(const RenderContextInstData& RCID, fxparam_constptr_t param, varval_t val){
  auto context          = RCID.rcfd()->GetTarget();
  auto FXI              = context->FXI();
  auto worldmatrix = RCID.worldMatrix();
  const auto& CPD       = RCID.rcfd()->topCPD();
  int W = CPD._width;
  int H = CPD._height;
  auto MTXI             = context->MTXI();
  const auto& RCFDPROPS = RCID.rcfd()->userProperties();
  bool is_picking       = CPD.isPicking();
  bool is_stereo        = CPD.isSinglePassStereo();
  auto pbrcommon = RCID.rcfd()->_pbrcommon;
  auto modcolor = context->RefModColor();

    ////////////////////////////////////////////////////////////
    // try to order these by commonalitiy
    //  or find a quicker dispatch method
    ////////////////////////////////////////////////////////////
    if (auto as_mtx4 = val.tryAs<fmtx4>()) {
      FXI->bindParamMatrix(param, as_mtx4.value());
    } else if (auto as_mtx4ptr = val.tryAs<fmtx4_ptr_t>()) {
      FXI->bindParamMatrix(param, *as_mtx4ptr.value().get());
    } else if (auto as_texture = val.tryAs<Texture*>()) {
      auto texture = as_texture.value();
      FXI->bindParamTexture(param, texture);
    } else if (auto as_texture = val.tryAs<texture_ptr_t>()) {
      auto texture = as_texture.value();
      FXI->bindParamTexture(param, texture.get());
    } else if (auto as_bool_ = val.tryAs<bool>()) {
      FXI->bindParamBool(param, as_bool_.value());
    } else if (auto as_int_ = val.tryAs<int>()) {
      FXI->bindParamInt(param, as_int_.value());
    } else if (auto as_float_ = val.tryAs<float>()) {
      FXI->bindParamFloat(param, as_float_.value());
    } else if (auto as_fvec4_ = val.tryAs<fvec4>()) {
      FXI->bindParamVect4(param, as_fvec4_.value());
    } else if (auto as_fvec3 = val.tryAs<fvec3>()) {
      FXI->bindParamVect3(param, as_fvec3.value());
    } else if (auto as_fvec2 = val.tryAs<fvec2>()) {
      FXI->bindParamVect2(param, as_fvec2.value());
    } else if (auto as_fmtx3 = val.tryAs<fmtx3>()) {
      FXI->bindParamMatrix(param, as_fmtx3.value());
    } else if (auto as_instancedata_ = val.tryAs<instanceddrawinstancedata_ptr_t>()) {
      OrkAssert(false);
    } else if (auto as_fquat = val.tryAs<fquat_ptr_t>()) {
      const auto& Q = *as_fquat.value().get();
      fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
      FXI->bindParamVect4(param, as_vec4);
    } else if (auto as_fplane3 = val.tryAs<fplane3_ptr_t>()) {
      const auto& P = *as_fplane3.value().get();
      fvec4 as_vec4(P.n, P.d);
      FXI->bindParamVect4(param, as_vec4);
    } 
    ///////////////////////////////////////////////////////////////////
    else if (auto as_varval_generator = val.tryAs<varval_generator_t>()) {
      auto gen = as_varval_generator.value();
      _set_typed_param(RCID,param,gen());
    }
    ///////////////////////////////////////////////////////////////////
    else if (auto as_storage = val.tryAs<FxShaderStorageBuffer*>()) {
      auto storage = as_storage.value();
      //FXI->bindParamStorageBuffer(param, storage);
      OrkAssert(false);
    }
    ///////////////////////////////////////////////////////////////////
    else if (auto as_crcstr = val.tryAs<crcstring_ptr_t>()) {
      const auto& crcstr = *as_crcstr.value().get();

      auto stereocams = CPD._stereo_cam_matrices;
      auto monocams   = CPD._mono_cam_matrices;

      switch (crcstr.hashed()) {

        case "RCID_PickID"_crcu: {
          auto itpfc = RCFDPROPS.find("pixel_fetch_context"_crc);
          OrkAssert(itpfc != RCFDPROPS.end());
          auto as_pfc = itpfc->second.get<pixelfetchctx_ptr_t>();
          auto as_u32 = as_pfc->encodeVariant(RCID._pickID);
          //printf( "PICKID: RGBA<%g %g %g %g>\n", as_rgba.x, as_rgba.y, as_rgba.z, as_rgba.w );
          FXI->bindParamU32(param, as_u32);
          break;
        }
        case "RCFD_Camera_Pick"_crcu: {
          auto it = RCFDPROPS.find("pickbufferMvpMatrix"_crc);
          OrkAssert(it != RCFDPROPS.end());
          auto as_mtx4p    = it->second.get<fmtx4_ptr_t>();
          const fmtx4& MVP = *(as_mtx4p.get());
          //MVP.dump("pickbufferMvpMatrix");
          FXI->bindParamMatrix(param, MVP);
          break;
        }
        case "RCFD_TIME"_crcu: {
          auto RCFD = RCID.rcfd();
          float time = RCFD->getUserProperty("time"_crc).get<float>();
          FXI->bindParamFloat(param, time);
          break;
        }
        case "CPD_Rtg_Dim"_crcu: {
          FXI->bindParamVect2(param, fvec2(W,H));
          break;
        }
        case "CPD_Rtg_InvDim"_crcu: {
          fvec2 invdim(1.0f/float(W), 1.0f/float(H));
          FXI->bindParamVect2(param, invdim);
          break;
        }
        case "FBI_RTG_DIM"_crcu: {
          auto rtg = context->FBI()->_active_rtgroup;
          int fbiw = rtg->miW;
          int fbih = rtg->miH;
          FXI->bindParamVect2(param, fvec2(fbiw,fbih));
          break;
        }
        case "FBI_RTG_INVDIM"_crcu: {
          auto rtg = context->FBI()->_active_rtgroup;
          int fbiw = rtg->miW;
          int fbih = rtg->miH;
          FXI->bindParamVect2(param, fvec2(1.0/fbiw,1.0/fbih));
          break;
        }
        case "RCFD_MODCOLOR"_crcu: {
          FXI->bindParamVect4(param, modcolor);
          break;
        }
        case "RCFD_M"_crcu: {
          FXI->bindParamMatrix(param, worldmatrix);
          break;
        }
        case "RCFD_DEPTH_MAP"_crcu: {
          auto RCFD = RCID.rcfd();
          auto depth_tex = RCFD->getUserProperty("DEPTH_MAP"_crc).get<texture_ptr_t>();
          FXI->bindParamTexture(param, depth_tex.get());
          //OrkAssert(false);
          break;
        }
        case "RCFD_EYE_POSITION"_crcu: {
          auto RCFD = RCID.rcfd();
          fmtx4 V;
          if (monocams) {
            V = monocams->_vmatrix;
          }          
          auto eyepos = V.inverse().translation();
          FXI->bindParamVect3(param, eyepos);
          //OrkAssert(false);
          break;
        }
        case "RCFD_PBR_BRDF_INTEGRATION_GGX"_crcu: {
          auto brdf_integration = pbrcommon->_irradianceMaps->_brdfIntegrationMapGGX.get();
          FXI->bindParamTexture(param, brdf_integration);
          break;
        }
        case "RCFD_PBR_DIFFUSE_ENV"_crcu: {
          auto the_tex = pbrcommon->envDiffuseTexture().get();
          FXI->bindParamTexture(param, the_tex);
          break;
        }
        case "RCFD_PBR_SPECULAR_ENV"_crcu: {
          auto the_tex = pbrcommon->envSpecularTexture().get();
          FXI->bindParamTexture(param, the_tex);
          break;
        }
        case "RCFD_PBR_BLACK_2DMAP"_crcu: {
          FXI->bindParamTexture(param, pbrcommon->_texBlack.get());
          break;
        }
        case "RCFD_PBR_WHITE_2DMAP"_crcu: {
          FXI->bindParamTexture(param, pbrcommon->_texWhite.get());
          break;
        }
        case "RCFD_PBR_WHITE_LIGHTMAP_ARRAY"_crcu: {
          FXI->bindParamTextureArray(param, pbrcommon->_texWhiteLightMapArray.get());
          break;
        }
        case "RCFD_PBR_LIGHTMAP_COLORS"_crcu: {
          static fvec3 lightmap_colors[8] = {
            fvec3(1.0f, 1.0f, 1.0f), // white
            fvec3(0.5f, 0.5f, 0.5f), // gray
            fvec3(1.0f, 0.5f, 0.5f), // red
            fvec3(0.5f, 1.0f, 0.5f), // green
            fvec3(0.5f, 0.5f, 1.0f), // blue
            fvec3(1.0f, 1.0f, 0.5f), // yellow
            fvec3(1.0f, 0.5f, 1.0f), // magenta
            fvec3(0.5f, 1.0f, 1.0f)  // cyan
          };
          FXI->bindParamVect3Array(param, lightmap_colors,8);
          break;
        }
        case "RCFD_PBR_BLACK_CUBEMAP"_crcu: {
          FXI->bindParamTexture(param, pbrcommon->_texCubeBlack.get());
          break;
        }
        case "RCFD_PBR_WHITE_CUBEMAP"_crcu: {
          FXI->bindParamTexture(param, pbrcommon->_texCubeWhite.get());
          break;
        }
        case "RCFD_MONOCAM_NEAR_FAR"_crcu: {
          float near = monocams->_camdat.mNear;
          float far = monocams->_camdat.mFar;
          FXI->bindParamVect2(param, fvec2(near, far));
          break;
        }
        case "RCFD_Camera_MVP_Mono"_crcu: {
          if (monocams) {
              //printf( "RCFD_Camera_MVP_Mono: monocams<%p>\n", (void*)monocams );
            FXI->bindParamMatrix(param, monocams->MVPMONO(worldmatrix));
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
            FXI->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_P_Mono"_crcu: {
          if (monocams) {
            FXI->bindParamMatrix(param, monocams->_pmatrix);
          } else {
            FXI->bindParamMatrix(param, MTXI->RefPMatrix());
          }
          break;
        }
        case "RCFD_Camera_VP_Mono"_crcu: {
          if (monocams) {
            fmtx4 vp = monocams->VPMONO();
            //vp.dump("monocams->VPMONO()");
            FXI->bindParamMatrix(param, vp);
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
            FXI->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_IV_Mono"_crcu: {
          if (monocams) {
            FXI->bindParamMatrix(param, monocams->GetIVMatrix());
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVMatrix().inverse());
            FXI->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_IVP_Mono"_crcu: {
          if (monocams) {
            auto VP = monocams->VPMONO();
            auto IVP = VP.inverse();
            //IVP.dump("IVP");
            FXI->bindParamMatrix(param, IVP );
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix().inverse());
            FXI->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_ZNORMAL_Mono"_crcu: {
          if (monocams) {
            auto VP = monocams->VPMONO();
            auto IVP = VP.inverse();
            fvec3 raydir = IVP.zNormal();
            //IVP.dump("IVP");
            FXI->bindParamVect3(param, raydir );
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix().inverse());
            FXI->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_VP_Left"_crcu: {
          if (is_stereo and stereocams) {
            FXI->bindParamMatrix(param, stereocams->VPL());
          }
          break;
        }
        case "RCFD_Camera_VP_Right"_crcu: {
          if (is_stereo and stereocams) {
            FXI->bindParamMatrix(param, stereocams->VPR());
          }
          break;
        }
        case "RCFD_Camera_IVP_Left"_crcu: {
          if (is_stereo and stereocams) {
            auto m = stereocams->VPL().inverse();
            FXI->bindParamMatrix(param, m);
          }
          break;
        }
        case "RCFD_Camera_IVP_Right"_crcu: {
          if (is_stereo and stereocams) {
            FXI->bindParamMatrix(param, stereocams->VPR().inverse());
          }
          break;
        }
        case "RCFD_Camera_MVP_Left"_crcu: {
          if (is_stereo and stereocams) {
            FXI->bindParamMatrix(param, stereocams->MVPL(worldmatrix));
          }
          break;
        }
        case "RCFD_Camera_MVP_Right"_crcu: {
          if (is_stereo and stereocams) {
            FXI->bindParamMatrix(param, stereocams->MVPR(worldmatrix));
          }
          break;
        }
        case "RCFD_Model_Rot"_crcu: {
          auto rotmtx = worldmatrix.rotMatrix33();
          FXI->bindParamMatrix(param, rotmtx);
          break;
        }
        case "RCFD_PBR_DPP_ZBIAS"_crcu: {
          FXI->bindParamFloat(param, pbrcommon->_dppZbias);
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
    } 
    else {
      OrkAssert(false);
    }
}
void FxPipeline::dump() const {
  printf( "FxPipeline<%p:%s>\n", (void*) this, _debugName.c_str() );
  __permutation.dump();
  printf( "  debugtext<%s>\n", _debugText.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::endBlock(const RenderContextInstData& RCID) {
  auto context = RCID.rcfd()->GetTarget();
  context->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const RenderContextInstData& RCID) const {
  auto RCFD       = RCID.rcfd();
  auto context    = RCFD->_target;
  auto fxi        = context->FXI();
  bool stereo = RCFD->hasCPD() ? RCFD->topCPD().isSinglePassStereo() : false;
  bool picking = RCFD->hasCPD() ? RCFD->topCPD().isPicking() : false;
  /////////////////
  FxPipelinePermutation permu;
  permu._stereo          = stereo;
  #if defined(__APPLE__)
  permu._stereo          = stereo;
  permu._vr_mono         = stereo;
  #endif

  permu._skinned         = RCID._isSkinned;
  permu._instanced       = RCID._isInstanced;
  permu._forced_technique = RCID._forced_technique;
  permu._is_picking = picking;
  permu._rendering_model = RCFD->_renderingmodel._modelID;
  //permu.dump();
  /////////////////
  return findPipeline(permu);
}
///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const FxPipelinePermutation& permu) const {
  fxpipeline_ptr_t pipeline;
  uint64_t index = permu.genIndex();
  auto it = _lut.find(index);
  if (it != _lut.end()) {
    pipeline = it->second;
  }
  else{ // miss
    OrkAssert(_on_miss);
    logchan_fxcache->log( "fxlut<%p> findPipeline onmiss index<%zu>", this, index );
    pipeline = _on_miss(permu);
    _lut[index] = pipeline;
  }
  return pipeline;
}///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
