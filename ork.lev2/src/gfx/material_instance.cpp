////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/material_instance.h>
#include <ork/lev2/gfx/renderer/renderable.h>

namespace ork::lev2 {
/////////////////////////////////////////////////////////////////////////
FxShaderTechniquePermutations::FxShaderTechniquePermutations() {
  _mono   = std::make_shared<FxShaderTechniquePermA>();
  _stereo = std::make_shared<FxShaderTechniquePermA>();
  _pick   = std::make_shared<FxShaderTechniquePermA>();
}
/////////////////////////////////////////////////////////////////////////
fxtechnique_constptr_t FxShaderTechniquePermutations::select(PermBase base, bool skinned, bool instanced) {
  fxshadertechperma_ptr_t base_permutation;
  switch (base) {
    case MONO:
      base_permutation = _mono;
      break;
    case STEREO:
      base_permutation = _stereo;
      break;
    case PICK:
      base_permutation = _pick;
      break;
    default:
      OrkAssert(false);
      break;
  }
  if (skinned)
    return instanced //
               ? base_permutation->_skinned_instanced
               : base_permutation->_skinned;
  else               // rigid
    return instanced //
               ? base_permutation->_rigid_instanced
               : base_permutation->_rigid;
}
/////////////////////////////////////////////////////////////////////////
GfxMaterialInstance::GfxMaterialInstance() {
  _teks = std::make_shared<FxShaderTechniquePermutations>();
}
/////////////////////////////////////////////////////////////////////////
void GfxMaterialInstance::wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall) {
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
int GfxMaterialInstance::beginBlock(const RenderContextInstData& RCID) {
  auto context    = RCID._RCFD->GetTarget();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();
  auto FXI        = context->FXI();
  // auto tek     = minst->valueForKey("technique").Get<fxtechnique_constptr_t>();

  FxShaderTechniquePermutations::PermBase baseperm;
  if (is_picking)
    baseperm = FxShaderTechniquePermutations::PermBase::PICK;
  else {
    baseperm = is_stereo //
                   ? FxShaderTechniquePermutations::PermBase::STEREO
                   : baseperm = FxShaderTechniquePermutations::PermBase::MONO;
  }
  auto tek = _teks->select(baseperm, false, false);
  return FXI->BeginBlock(tek, RCID);
}
///////////////////////////////////////////////////////////////////////////////
bool GfxMaterialInstance::beginPass(const RenderContextInstData& RCID, int ipass) {
  auto context    = RCID._RCFD->GetTarget();
  auto MTXI       = context->MTXI();
  auto FXI        = context->FXI();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();

  bool rval = FXI->BindPass(ipass);
  if (not rval)
    return rval;

  const auto& worldmatrix = RCID._dagrenderable //
                                ? RCID._dagrenderable->_worldMatrix
                                : MTXI->RefMMatrix();

  auto stereocams = CPD._stereoCameraMatrices;
  auto monocams   = CPD._cameraMatrices;

  for (auto item : _params) {
    fxparam_constptr_t param = item.first;
    const auto& val          = item.second;
    if (auto as_mtx4 = val.TryAs<fmtx4_ptr_t>()) {
      FXI->BindParamMatrix(param, *as_mtx4.value().get());
    } else if (auto as_crcstr = val.TryAs<crcstring_ptr_t>()) {
      const auto& crcstr = *as_crcstr.value().get();
      switch (crcstr.hashed()) {
        case "RCFD_Camera_MVP_Mono"_crcu: {
          if (monocams) {
            FXI->BindParamMatrix(param, monocams->MVPMONO(worldmatrix));
          } else {
            auto MVP = worldmatrix * MTXI->RefVPMatrix();
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
        default:
          OrkAssert(false);
          break;
      }

    } else if (auto as_instancedata_ = val.TryAs<instanceddrawdata_ptr_t>()) {
      OrkAssert(false);
    } else if (auto as_fvec4_ = val.TryAs<fvec4_ptr_t>()) {
      FXI->BindParamVect4(param, *as_fvec4_.value().get());
    } else if (auto as_fvec3 = val.TryAs<fvec3_ptr_t>()) {
      FXI->BindParamVect3(param, *as_fvec3.value().get());
    } else if (auto as_fvec2 = val.TryAs<fvec2_ptr_t>()) {
      FXI->BindParamVect2(param, *as_fvec2.value().get());
    } else if (auto as_fmtx3 = val.TryAs<fmtx3_ptr_t>()) {
      FXI->BindParamMatrix(param, *as_fmtx3.value().get());
    } else if (auto as_fquat = val.TryAs<fquat_ptr_t>()) {
      const auto& Q = *as_fquat.value().get();
      fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
      FXI->BindParamVect4(param, as_vec4);
    } else if (auto as_fplane3 = val.TryAs<fplane3_ptr_t>()) {
      const auto& P = *as_fplane3.value().get();
      fvec4 as_vec4(P.n, P.d);
      FXI->BindParamVect4(param, as_vec4);
    } else if (auto as_texture = val.TryAs<Texture*>()) {
      auto texture = as_texture.value();
      FXI->BindParamCTex(param, texture);
    } else {
      OrkAssert(false);
    }
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void GfxMaterialInstance::endPass(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  context->FXI()->EndPass();
}
///////////////////////////////////////////////////////////////////////////////
void GfxMaterialInstance::endBlock(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  context->FXI()->EndBlock();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
