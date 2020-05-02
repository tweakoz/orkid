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
// GfxMaterialInstance::GfxMaterialInstance(material_ptr_t mtl)
//  : _material(mtl) {
//}
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
////////////////////////////////////////////
void GfxMaterialInstance::setInstanceMvpParams(
    std::string monocam, //
    std::string stereocamL,
    std::string stereocamR) {
  /*if (auto mvp_mono = this->param(monocam)) {
    crcstring_ptr_t tok_mono = std::make_shared<CrcString>("RCFD_Camera_MVP_Mono");
    materialinst->_params[mvp_mono].Set<crcstring_ptr_t>(tok_mono);
    printf("tok_mono<0x%zx:%zu>\n", tok_mono->hashed(), tok_mono->hashed());
  }
  if (auto mvp_left = this->param(stereocamL)) {
    crcstring_ptr_t tok_stereoL = std::make_shared<CrcString>("RCFD_Camera_MVP_Left");
    materialinst->_params[mvp_left].Set<crcstring_ptr_t>(tok_stereoL);
    printf("tok_stereoL<0x%zx:%zu>\n", tok_stereoL->hashed(), tok_stereoL->hashed());
  }
  if (auto mvp_right = this->param(stereocamR)) {
    crcstring_ptr_t tok_stereoR = std::make_shared<CrcString>("RCFD_Camera_MVP_Right");
    materialinst->_params[mvp_right].Set<crcstring_ptr_t>(tok_stereoR);
    printf("tok_stereoR<0x%zx:%zu>\n", tok_stereoR->hashed(), tok_stereoR->hashed());
  }*/
}
///////////////////////////////////////////////////////////////////////////////
int GfxMaterialInstance::beginBlock(const RenderContextInstData& RCID) {
  auto context    = RCID._RCFD->GetTarget();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();
  auto FXI        = context->FXI();
  // auto tek     = minst->valueForKey("technique").Get<fxtechnique_constptr_t>();
  auto tek = is_stereo ? _stereoTek : _monoTek;
  if (is_picking) {
    if (_pickTek)
      return FXI->BeginBlock(_pickTek, RCID);
  } else {
    if (is_stereo and _stereoTek)
      return FXI->BeginBlock(_stereoTek, RCID);
    else if (_monoTek)
      return FXI->BeginBlock(_monoTek, RCID);
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
bool GfxMaterialInstance::beginPass(const RenderContextInstData& RCID, int ipass) {
  auto context    = RCID._RCFD->GetTarget();
  auto MTXI       = context->MTXI();
  auto FXI        = context->FXI();
  const auto& CPD = RCID._RCFD->topCPD();
  bool is_picking = CPD.isPicking();
  bool is_stereo  = CPD.isStereoOnePass();

  bool rval = false; // this->BeginPass(context, ipass);

  const auto& worldmatrix = RCID._dagrenderable //
                                ? RCID._dagrenderable->_worldMatrix
                                : MTXI->RefMMatrix();

  auto stereocams = CPD._stereoCameraMatrices;
  auto monocams   = CPD._cameraMatrices;

  for (auto item : _params) {
    fxparam_constptr_t param = item.first;
    const auto& val          = item.second;
    if (auto as_mtx4 = val.TryAs<fmtx4_ptr_t>()) {
      // this->bindParamMatrix(param, *as_mtx4.value().get());
    } else if (auto as_crcstr = val.TryAs<crcstring_ptr_t>()) {
      const auto& crcstr = *as_crcstr.value().get();
      switch (crcstr.hashed()) {
        case "RCFD_Camera_MVP_Mono"_crcu: {
          if (monocams) {
            // this->bindParamMatrix(param, monocams->MVPMONO(worldmatrix));
          } else {
            auto MVP = worldmatrix * MTXI->RefVPMatrix();
            // this->bindParamMatrix(param, MVP);
          }
          break;
        }
        case "RCFD_Camera_MVP_Left"_crcu: {
          if (is_stereo and stereocams) {
            // this->bindParamMatrix(param, stereocams->MVPL(worldmatrix));
          }
          break;
        }
        case "RCFD_Camera_MVP_Right"_crcu: {
          if (is_stereo and stereocams) {
            // this->bindParamMatrix(param, stereocams->MVPR(worldmatrix));
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
      // this->bindParamVec4(param, *as_fvec4_.value().get());
    } else if (auto as_fvec3 = val.TryAs<fvec3_ptr_t>()) {
      // this->bindParamVec3(param, *as_fvec3.value().get());
    } else if (auto as_fvec2 = val.TryAs<fvec2_ptr_t>()) {
      // this->bindParamVec2(param, *as_fvec2.value().get());
    } else if (auto as_fmtx3 = val.TryAs<fmtx3_ptr_t>()) {
      // this->bindParamMatrix(param, *as_fmtx3.value().get());
    } else if (auto as_fquat = val.TryAs<fquat_ptr_t>()) {
      const auto& Q = *as_fquat.value().get();
      fvec4 as_vec4(Q.x, Q.y, Q.z, Q.w);
      // this->bindParamVec4(param, as_vec4);
    } else if (auto as_fplane3 = val.TryAs<fplane3_ptr_t>()) {
      const auto& P = *as_fplane3.value().get();
      fvec4 as_vec4(P.n, P.d);
      // this->bindParamVec4(param, as_vec4);
    } else if (auto as_texture = val.TryAs<Texture*>()) {
      auto texture = as_texture.value();
      // this->bindParamCTex(param, texture);
    } else {
      OrkAssert(false);
    }
  }
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void GfxMaterialInstance::endPass(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  // this->EndPass(context);
}
///////////////////////////////////////////////////////////////////////////////
void GfxMaterialInstance::endBlock(const RenderContextInstData& RCID) {
  auto context = RCID._RCFD->GetTarget();
  // this->EndBlock(context);
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
