////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/fx_pipeline.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/util/logger.h>

namespace ork::lev2 {
static logchannel_ptr_t logchan_fxcache = logger()->createChannel("fxcache",fvec3(0.7,0.7,0.5),false);
///////////////////////////////////////////////////////////////////////////////
uint64_t FxPipelinePermutation::genIndex() const {
  uint64_t index = 0;
  index += (uint64_t(_stereo) << 1);
  index += (uint64_t(_instanced) << 2);
  index += (uint64_t(_skinned) << 3);
  index += (uint64_t(_rendering_model) << 4);

  auto tekovr = uint64_t((const void*)_forced_technique);
  index += tekovr;
  return index;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipelinePermutation::dump() const {
  logchan_fxcache->log(
      "configdump: rendering_model<0x%zx> stereo<%d> instanced<%d> skinned<%d>",
      uint64_t(_rendering_model),
      int(_stereo),
      int(_instanced),
      int(_skinned));
}
///////////////////////////////////////////////////////////////////////////////
void FxPipelinePermutationSet::add(fxpipelinepermutation_constptr_t perm){
  __permutations.insert(perm);
}
/////////////////////////////////////////////////////////////////////////
FxPipeline::FxPipeline(const FxPipelinePermutation& config)
    : __permutation(config) {
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::bindParam(fxparam_constptr_t p, varval_t v){
  OrkAssert(p!=nullptr);
  _params[p] = v;
}
/////////////////////////////////////////////////////////////////////////
void FxPipeline::wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall) {
  int inumpasses = beginBlock(RCID);
  for (int ipass = 0; ipass < inumpasses; ipass++) {
    if (beginPass(RCID, ipass)) {
      drawcall();
      endPass(RCID);
    }
  }
  endBlock(RCID);
}
///////////////////////////////////////////////////////////////////////////////
int FxPipeline::beginBlock(const RenderContextInstData& RCID) {
  auto context    = RCID._RCFD->GetTarget();
  auto FXI        = context->FXI();
  return FXI->BeginBlock(_technique, RCID);
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::_set_typed_param(const RenderContextInstData& RCID, fxparam_constptr_t param, varval_t val){
  auto context          = RCID._RCFD->GetTarget();
  auto FXI              = context->FXI();
  auto worldmatrix = RCID.worldMatrix();
  const auto& CPD       = RCID._RCFD->topCPD();
  int W = CPD._width;
  int H = CPD._height;
  auto MTXI             = context->MTXI();
  auto RSI              = context->RSI();
  const auto& RCFDPROPS = RCID._RCFD->userProperties();
  bool is_picking       = CPD.isPicking();
  bool is_stereo        = CPD.isStereoOnePass();
  auto pbrcommon = RCID._RCFD->_pbrcommon;
  auto modcolor = context->RefModColor();

    ////////////////////////////////////////////////////////////
    // try to order these by commonalitiy
    //  or find a quicker dispatch method
    ////////////////////////////////////////////////////////////
    if (auto as_mtx4 = val.tryAs<fmtx4>()) {
      FXI->BindParamMatrix(param, as_mtx4.value());
    } else if (auto as_mtx4ptr = val.tryAs<fmtx4_ptr_t>()) {
      FXI->BindParamMatrix(param, *as_mtx4ptr.value().get());
    } else if (auto as_texture = val.tryAs<Texture*>()) {
      auto texture = as_texture.value();
      FXI->BindParamCTex(param, texture);
    } else if (auto as_texture = val.tryAs<texture_ptr_t>()) {
      auto texture = as_texture.value();
      FXI->BindParamCTex(param, texture.get());
    }
    else if (auto as_float_ = val.tryAs<float>()) {
      FXI->BindParamFloat(param, as_float_.value());
    } else if (auto as_fvec4_ = val.tryAs<fvec4>()) {
      FXI->BindParamVect4(param, as_fvec4_.value());
    } else if (auto as_fvec3 = val.tryAs<fvec3>()) {
      FXI->BindParamVect3(param, as_fvec3.value());
    } else if (auto as_fvec2 = val.tryAs<fvec2>()) {
      FXI->BindParamVect2(param, as_fvec2.value());
    } else if (auto as_fmtx3 = val.tryAs<fmtx3>()) {
      FXI->BindParamMatrix(param, as_fmtx3.value());
    } else if (auto as_instancedata_ = val.tryAs<instanceddrawinstancedata_ptr_t>()) {
      OrkAssert(false);
    } else if (auto as_fquat = val.tryAs<fquat_ptr_t>()) {
      const auto& Q = *as_fquat.value().get();
      fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
      FXI->BindParamVect4(param, as_vec4);
    } else if (auto as_fplane3 = val.tryAs<fplane3_ptr_t>()) {
      const auto& P = *as_fplane3.value().get();
      fvec4 as_vec4(P.n, P.d);
      FXI->BindParamVect4(param, as_vec4);
    } 
    ///////////////////////////////////////////////////////////////////
    else if (auto as_varval_generator = val.tryAs<varval_generator_t>()) {
      auto gen = as_varval_generator.value();
      _set_typed_param(RCID,param,gen());
    }
    ///////////////////////////////////////////////////////////////////
    else if (auto as_crcstr = val.tryAs<crcstring_ptr_t>()) {
      const auto& crcstr = *as_crcstr.value().get();

      auto stereocams = CPD._stereoCameraMatrices;
      auto monocams   = CPD._cameraMatrices;

      switch (crcstr.hashed()) {

        case "RCFD_Camera_Pick"_crcu: {
          auto it = RCFDPROPS.find("pickbufferMvpMatrix"_crc);
          OrkAssert(it != RCFDPROPS.end());
          auto as_mtx4p    = it->second.get<fmtx4_ptr_t>();
          const fmtx4& MVP = *(as_mtx4p.get());
          FXI->BindParamMatrix(param, MVP);
          break;
        }
        case "RCFD_TIME"_crcu: {
          auto RCFD = RCID._RCFD;
          float time = RCFD->getUserProperty("time"_crc).get<float>();
          FXI->BindParamFloat(param, time);
          break;
        }
        case "CPD_Rtg_Dim"_crcu: {
          FXI->BindParamVect2(param, fvec2(W,H));
          break;
        }
        case "CPD_Rtg_InvDim"_crcu: {
          FXI->BindParamVect2(param, fvec2(1.0f/float(W),1.0f/float(H)));
          break;
        }
        case "RCFD_MODCOLOR"_crcu: {
          FXI->BindParamVect4(param, modcolor);
          break;
        }
        case "RCFD_M"_crcu: {
          FXI->BindParamMatrix(param, worldmatrix);
          break;
        }
        case "RCFD_Camera_MVP_Mono"_crcu: {
          if (monocams) {
            FXI->BindParamMatrix(param, monocams->MVPMONO(worldmatrix));
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
            FXI->BindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_VP_Mono"_crcu: {
          if (monocams) {
            FXI->BindParamMatrix(param, monocams->VPMONO());
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix());
            FXI->BindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_IV_Mono"_crcu: {
          if (monocams) {
            FXI->BindParamMatrix(param, monocams->GetIVMatrix());
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVMatrix().inverse());
            FXI->BindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_IVP_Mono"_crcu: {
          if (monocams) {
            FXI->BindParamMatrix(param, monocams->VPMONO().inverse());
          } else {
            auto MVP = fmtx4::multiply_ltor(worldmatrix, MTXI->RefVPMatrix().inverse());
            FXI->BindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_VP_Left"_crcu: {
          if (is_stereo and stereocams) {
            FXI->BindParamMatrix(param, stereocams->VPL());
          }
          break;
        }
        case "RCFD_Camera_VP_Right"_crcu: {
          if (is_stereo and stereocams) {
            FXI->BindParamMatrix(param, stereocams->VPR());
          }
          break;
        }
        case "RCFD_Camera_IVP_Left"_crcu: {
          if (is_stereo and stereocams) {
            FXI->BindParamMatrix(param, stereocams->VPL().inverse());
          }
          break;
        }
        case "RCFD_Camera_IVP_Right"_crcu: {
          if (is_stereo and stereocams) {
            FXI->BindParamMatrix(param, stereocams->VPR().inverse());
          }
          break;
        }
        case "RCFD_Camera_MVP_Left"_crcu: {
          if (is_stereo and stereocams) {
            FXI->BindParamMatrix(param, stereocams->MVPL(worldmatrix));
          }
          break;
        }
        case "RCFD_Camera_MVP_Right"_crcu: {
          if (is_stereo and stereocams) {
            FXI->BindParamMatrix(param, stereocams->MVPR(worldmatrix));
          }
          break;
        }
        case "RCFD_Model_Rot"_crcu: {
          auto rotmtx = worldmatrix.rotMatrix33();
          FXI->BindParamMatrix(param, rotmtx);
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
///////////////////////////////////////////////////////////////////////////////
bool FxPipeline::beginPass(const RenderContextInstData& RCID, int ipass) {
  auto context          = RCID._RCFD->GetTarget();
  auto FXI              = context->FXI();

  bool rval = FXI->BindPass(ipass);
  if (not rval)
    return rval;

  ///////////////////////////////
  // run state lambdas
  ///////////////////////////////

  for( auto& item: _statelambdas ){
    item(RCID,ipass);
  }

  ///////////////////////////////
  // run individual state items
  ///////////////////////////////


  for (auto item : _params) {
    fxparam_constptr_t param = item.first;
    const auto& val          = item.second;
    _set_typed_param(RCID,param,val);
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::endPass(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  context->FXI()->EndPass();
}
///////////////////////////////////////////////////////////////////////////////
void FxPipeline::endBlock(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  context->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
fxpipeline_ptr_t FxPipelineCache::findPipeline(const RenderContextInstData& RCID) const {
  auto RCFD       = RCID._RCFD;
  auto context    = RCFD->_target;
  auto fxi        = context->FXI();
  bool stereo = RCFD->hasCPD() ? RCFD->topCPD().isStereoOnePass() : false;
  /////////////////
  FxPipelinePermutation permu;
  permu._stereo          = stereo;
  permu._skinned         = RCID._isSkinned;
  permu._instanced       = RCID._isInstanced;
  permu._forced_technique = RCID._forced_technique;
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