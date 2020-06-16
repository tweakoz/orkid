////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxenv.h>

using namespace ork::lev2;

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template <typename VPTYPE> void UIToolHandler<VPTYPE>::Attach(VPTYPE* pvp) {
  this->mpViewport = pvp;
  DoAttach(pvp);
}

template <typename VPTYPE> void UIToolHandler<VPTYPE>::Detach(VPTYPE* pvp) {
  DoDetach(pvp);
}

template <typename VPTYPE> ui::HandlerResult UIToolHandler<VPTYPE>::DoOnUiEvent(ui::event_constptr_t EV) {
  ui::HandlerResult ret;
  switch (EV->_eventcode) {
    case ui::EventCode::KEY: {
      switch (EV->miKeyCode) {
        case 13:
          if (mState < int(mpSubIconNameVector.size() - 1))
            SetState(mState + 1);
          else
            SetState(0);
          ret.setHandled(this);
          break;
      }
    }
  }

  return ret;
}

///////////////////////////////////////////////////////////////////////////////

template <typename VPTYPE> void UIToolHandler<VPTYPE>::SetBaseIconName(std::string name) {
  mBaseIconName = name;
  LoadToolIcon();
}

template <typename VPTYPE> void UIToolHandler<VPTYPE>::LoadToolIcon() {
  if (0 == mpBaseIcon) {
    auto texasset = asset::AssetManager<ork::lev2::TextureAsset>::Load(mBaseIconName.c_str());
    mpBaseIcon    = (texasset == nullptr) ? nullptr : texasset->GetTexture();
    for (auto subicon_name : mpSubIconNameVector) {
      texasset = asset::AssetManager<ork::lev2::TextureAsset>::Load(subicon_name.c_str());
      if (texasset)
        mpSubIconVector.push_back(texasset->GetTexture());
    }
  }
}

template <typename VPTYPE> void UIToolHandler<VPTYPE>::DrawToolIcon(lev2::Context* pTARG, int ix, int iy, bool bhilite) {
  if (mpBaseIcon) {
    lev2::DynamicVertexBuffer<lev2::SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

    lev2::GfxMaterialUITextured* texmaterial = nullptr;
    if (auto try_mtl = mpBaseIcon->_varmap.typedValueForKey<lev2::GfxMaterialUITextured*>("iconmtl")) {
      texmaterial = try_mtl.value();
    } else {
      texmaterial = //
          new lev2::GfxMaterialUITextured(pTARG);
      texmaterial->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_OFF);
      texmaterial->SetTexture(lev2::ETEXDEST_DIFFUSE, mpBaseIcon);
      texmaterial->_rasterstate.SetBlending(lev2::EBLENDING_OFF);
      texmaterial->_rasterstate.SetAlphaTest(lev2::EALPHATEST_OFF, 0.0f);
      texmaterial->_rasterstate.SetDepthTest(lev2::EDEPTHTEST_ALWAYS);
      mpBaseIcon->_varmap.makeValueForKey<lev2::GfxMaterialUITextured*>("iconmtl") = texmaterial;
    }
    lev2::GfxMaterialUI UiMat(pTARG);
    int icount = bhilite ? 12 : 6;
    int ibase  = vb.GetNumVertices();
    float fx0, fy0, fx1, fy1;
    float fu0, fv0, fu1, fv1;
    float fz = 0.0f;
    fu0      = 0.0f;
    fv0      = 0.0f;
    fu1      = 1.0f;
    fv1      = 1.0f;
    ////////////////////////////////
    VtxWriter<SVtxV12C4T16> vwriter;
    vwriter.Lock(pTARG, &vb, icount);
    {
      if (bhilite) {
        fx0 = float(ix - 1);
        fx1 = float(ix + 33);
        fy0 = float(iy - 1);
        fy1 = float(iy + 33);
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx0, fy0, fz, fu0, fv0));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx1, fy0, fz, fu1, fv0));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx1, fy1, fz, fu1, fv1));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx0, fy0, fz, fu0, fv0));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx1, fy1, fz, fu1, fv1));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx0, fy1, fz, fu0, fv1));
      }
      {
        fx0 = float(ix);
        fx1 = float(ix + 32);
        fy0 = float(iy);
        fy1 = float(iy + 32);
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx0, fy0, fz, fu0, fv0));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx1, fy0, fz, fu1, fv0));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx1, fy1, fz, fu1, fv1));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx0, fy0, fz, fu0, fv0));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx1, fy1, fz, fu1, fv1));
        vwriter.AddVertex(lev2::SVtxV12C4T16(fx0, fy1, fz, fu0, fv1));
      }
    }
    vwriter.UnLock(pTARG);
    pTARG->MTXI()->PushUIMatrix();
    {
      ////////////////////////////////
      if (bhilite) {
        UiMat.SetTexture(lev2::ETEXDEST_DIFFUSE, 0);
        UiMat._rasterstate.SetBlending(lev2::EBLENDING_OFF);
        pTARG->PushModColor(fcolor4::Green());
        pTARG->GBI()->DrawPrimitive(&UiMat, vb, lev2::EPrimitiveType::TRIANGLES, ibase, 6);
        pTARG->PopModColor();
      }
      ////////////////////////////////
      pTARG->PushModColor(fcolor4::White());
      pTARG->GBI()->DrawPrimitive(texmaterial, vb, lev2::EPrimitiveType::TRIANGLES, bhilite ? ibase + 6 : ibase, 6);
      pTARG->PopModColor();
    }
    pTARG->MTXI()->PopUIMatrix();
    ////////////////////////////////
  }
  DrawSubToolIcon(pTARG, ix + 36, iy, bhilite);
}

///////////////////////////////////////////////////////////////////////////////

template <typename VPTYPE> void UIToolHandler<VPTYPE>::DrawSubToolIcon(lev2::Context* pTARG, int ix, int iy, bool bhilite) {
  if (mState < 0 || mState >= int(mpSubIconVector.size()))
    return;

  ork::lev2::Texture* decorator = mpSubIconVector[mState];
  if (decorator) {
    static lev2::GfxMaterialUITextured UiMatTex(pTARG);

    UiMatTex.SetTexture(lev2::ETEXDEST_DIFFUSE, decorator);
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename VPTYPE> void UIToolHandler<VPTYPE>::SetState(int state) {
  if (state >= 0 && state < int(mpSubIconNameVector.size())) {
    OnExit(mState);
    mState = state;
    OnEnter(mState);
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename VPTYPE>
UIToolHandler<VPTYPE>::UIToolHandler()
    : Widget("uitoolh", 0, 0, 0, 0)
    , mpBaseIcon(0)
    , mState(0)
    , mpViewport(0) {
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::tool
