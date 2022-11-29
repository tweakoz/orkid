////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/fxstate_instance.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/util/logger.h>

namespace ork::lev2 {
static logchannel_ptr_t logchan_fxcache = logger()->createChannel("fxcache",fvec3(0.7,0.7,0.5));
///////////////////////////////////////////////////////////////////////////////
uint64_t FxCachePermutation::genIndex() const {
  uint64_t index = 0;
  index += (uint64_t(_stereo) << 1);
  index += (uint64_t(_instanced) << 2);
  index += (uint64_t(_skinned) << 3);
  index += (uint64_t(_rendering_model) << 4);
  return index;
}
///////////////////////////////////////////////////////////////////////////////
void FxCachePermutation::dump() const {
  logchan_fxcache->log(
      "configdump: rendering_model<0x%zx> stereo<%d> instanced<%d> skinned<%d>",
      uint64_t(_rendering_model),
      int(_stereo),
      int(_instanced),
      int(_skinned));
}
///////////////////////////////////////////////////////////////////////////////
void FxCachePermutationSet::add(fxcachepermutation_constptr_t perm){
  __permutations.insert(perm);
}
/////////////////////////////////////////////////////////////////////////
FxStateInstance::FxStateInstance(const FxCachePermutation& config)
    : __permutation(config) {
}
/////////////////////////////////////////////////////////////////////////
void FxStateInstance::wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall) {
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
int FxStateInstance::beginBlock(const RenderContextInstData& RCID) {
  auto context    = RCID._RCFD->GetTarget();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();
  auto FXI        = context->FXI();
  return FXI->BeginBlock(_technique, RCID);
}
///////////////////////////////////////////////////////////////////////////////
bool FxStateInstance::beginPass(const RenderContextInstData& RCID, int ipass) {
  auto context          = RCID._RCFD->GetTarget();
  auto MTXI             = context->MTXI();
  auto FXI              = context->FXI();
  auto RSI              = context->RSI();
  const auto& CPD       = RCID._RCFD->topCPD();
  const auto& RCFDPROPS = RCID._RCFD->userProperties();
  bool is_picking       = CPD.isPicking();
  bool is_stereo        = CPD.isStereoOnePass();
  auto pbrcommon = RCID._RCFD->_pbrcommon;

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

  auto worldmatrix = RCID.worldMatrix();

  auto stereocams = CPD._stereoCameraMatrices;
  auto monocams   = CPD._cameraMatrices;


  for (auto item : _params) {
    fxparam_constptr_t param = item.first;
    const auto& val          = item.second;
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
    } else if (auto as_crcstr = val.tryAs<crcstring_ptr_t>()) {
      const auto& crcstr = *as_crcstr.value().get();
      switch (crcstr.hashed()) {

        case "RCFD_Camera_Pick"_crcu: {
          auto it = RCFDPROPS.find("pickbufferMvpMatrix"_crc);
          OrkAssert(it != RCFDPROPS.end());
          auto as_mtx4p    = it->second.get<fmtx4_ptr_t>();
          const fmtx4& MVP = *(as_mtx4p.get());
          FXI->BindParamMatrix(param, MVP);
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
    } else if (auto as_float_ = val.tryAs<float>()) {
      FXI->BindParamFloat(param, as_float_.value());
    } else if (auto as_fvec4_ = val.tryAs<fvec4>()) {
      FXI->BindParamVect4(param, as_fvec4_.value());
    } else if (auto as_fvec4_ = val.tryAs<fvec4_ptr_t>()) {
      FXI->BindParamVect4(param, *as_fvec4_.value().get());
    } else if (auto as_fvec3 = val.tryAs<fvec3_ptr_t>()) {
      FXI->BindParamVect3(param, *as_fvec3.value().get());
    } else if (auto as_fvec2 = val.tryAs<fvec2_ptr_t>()) {
      FXI->BindParamVect2(param, *as_fvec2.value().get());
    } else if (auto as_fmtx3 = val.tryAs<fmtx3_ptr_t>()) {
      FXI->BindParamMatrix(param, *as_fmtx3.value().get());
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
    } else {
      OrkAssert(false);
    }
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void FxStateInstance::endPass(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  context->FXI()->EndPass();
}
///////////////////////////////////////////////////////////////////////////////
void FxStateInstance::endBlock(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  context->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
fxinstance_ptr_t FxStateInstanceCache::findfxinst(const RenderContextInstData& RCID) const {
  auto RCFD       = RCID._RCFD;
  auto context    = RCFD->_target;
  auto fxi        = context->FXI();
  const auto& CPD = RCFD->topCPD();
  /////////////////
  FxCachePermutation perm;
  perm._stereo          = CPD.isStereoOnePass();
  perm._skinned         = RCID._isSkinned;
  perm._instanced       = RCID._isInstanced;
  perm._rendering_model = RCFD->_renderingmodel._modelID;
  //perm.dump();
  /////////////////
  fxinstance_ptr_t fxinst;
  /////////////////
  uint64_t index = perm.genIndex();
  auto it = _lut.find(index);
  if (it != _lut.end()) {
    fxinst = it->second;
  }
  else{ // miss
    OrkAssert(_on_miss);
    logchan_fxcache->log( "fxlut<%p> findfxinst onmiss index<%zu>", this, index );
    fxinst = _on_miss(perm);
    _lut[index] = fxinst;
  }
  /////////////////
  return fxinst;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
