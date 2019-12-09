////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/misc_math.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IProperty.h>
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/qtui/qtmainwin.h>

using namespace ork::lev2;

//////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
//////////////////////////////////////////////////////////////////////////////

typedef SVtxV16T16C16 vtx_t;

GedSkin::GedSkin(GfxTarget* ptarg) : miScrollY(0), mpCurrentGedVp(nullptr), mpFONT(nullptr), miCHARW(0), miCHARH(0) {}

struct GedText {
  typedef ork::ArrayString<128> StringType;

  int ix, iy;
  StringType mString;
};

//'ork::tool::ged::GedSkin::PrimContainer *' to
//'ork::tool::ged::GedSkin::PrimContainer *'

void GedSkin::AddPrim(const GedPrim& cb) {
  int isort = calcsort(cb.miSortKey);
  PrimContainers::iterator it = mPrimContainers.find(isort);
  if (it == mPrimContainers.end()) {
    PrimContainer* pcontainer = mPrimContainerPool.allocate();
    OrkAssert(pcontainer != 0);
    it = mPrimContainers.AddSorted(isort, pcontainer);
  }

  PrimContainer* pctr = it->second;
  if (cb.mDrawCB) {
    GedPrim* pooledprim = pctr->mPrimPool.allocate();
    *pooledprim = cb;
    pctr->mCustomPrims.push_back(pooledprim);
  } else
    switch (cb.meType) {
      case EPRIM_LINES: {
        GedPrim* pooledprim = pctr->mPrimPool.allocate();
        *pooledprim = cb;
        pctr->mLinePrims.push_back(pooledprim);
        break;
      }
      case EPRIM_QUADS: {
        GedPrim* pooledprim = pctr->mPrimPool.allocate();
        *pooledprim = cb;
        pctr->mQuadPrims.push_back(pooledprim);
        break;
      }
    }
}

void GedSkin::PrimContainer::clear() {
  mPrimPool.clear();
  mLinePrims.clear();
  mQuadPrims.clear();
  mCustomPrims.clear();
}

void GedSkin::clear() {
  for (int i = 0; i < int(mPrimContainerPool.capacity()); i++) {
    mPrimContainerPool.direct_access(i).clear();
  }
  mPrimContainerPool.clear();
  mPrimContainers.clear();
}

///////////////////////////////////////////////////////////////////////////////
struct GedSkin0 : public GedSkin { ///////////////////////////////////////////////////////////////////
  bool mbPickMode;
  orkvector<GedText> mTexts;
  GedSkin0(GfxTarget* ptarg) : GedSkin(ptarg) {
    mpFONT = lev2::FontMan::GetFont("i14");
    miCHARW = mpFONT->GetFontDesc().miAdvanceWidth;
    miCHARH = mpFONT->GetFontDesc().miAdvanceHeight;
  }
  ///////////////////////////////////////////////////////////////////
  fvec4 GetStyleColor(GedObject* pnode, ESTYLE ic) {
    GedMapNode* pmapnode = rtti::autocast(pnode);

    bool bismapnode = (pmapnode != 0);

    fvec3 color;
    bool bsc = true;
    bool balternate = true;
    switch (ic) {
      case ESTYLE_BACKGROUND_1:
        color = bismapnode ? fvec3(0.8f, 0.8f, 0.78f) : fvec3(0.6f, 0.6f, 0.58f);
        break;
      case ESTYLE_BACKGROUND_2:
        color = fvec3(0.9f, 0.9f, 0.88f);
        break;
      case ESTYLE_BACKGROUND_3:
        color = fvec3(1.0f, 0.7f, 0.7f);
        break;
      case ESTYLE_BACKGROUND_4:
        color = fvec3(0.8f, 0.5f, 0.8f);
        break;
      case ESTYLE_BACKGROUND_5:
        color = fvec3(0.8f, 0.8f, 0.5f);
        break;
      case ESTYLE_BACKGROUND_OPS:
        color = fvec3(0.8f, 0.4f, 0.4f);
        balternate = false;
        break;
      case ESTYLE_BACKGROUND_OBJNODE_LABEL:
        color = fvec3(0.4f, 0.4f, 1.0f);
        balternate = false;
        break;
      case ESTYLE_BACKGROUND_GROUP_LABEL:
        color = fvec3(0.5f, 0.5f, 0.7f);
        balternate = false;
        break;
      case ESTYLE_BACKGROUND_MAPNODE_LABEL:
        color = fvec3(0.7f, 0.5f, 0.5f);
        balternate = false;
        break;
      case ESTYLE_DEFAULT_HIGHLIGHT:
        color = fvec3(0.85f, 0.82f, 0.8f);
        break;
      case ESTYLE_DEFAULT_OUTLINE:
        color = fvec3(0.5f, 0.5f, 0.5f);
        break;
      case ESTYLE_DEFAULT_CHECKBOX:
        color = fvec3(0.0f, 0.0f, 0.0f);
        bsc = false;
        break;
      case ESTYLE_BUTTON_OUTLINE:
        color = fvec3(0.0f, 0.0f, 0.0f);
        bsc = false;
        break;
      default:
        color = fvec3(1.0f, 0.0f, 0.0f);
        break;
    }

    float fdepth = (bsc ? float(pnode->GetDepth()) / 16.0f : 1.0f);

    if (fdepth > 0.9f)
      fdepth = 0.9f;

    fdepth = (1.0f - fdepth);

    fdepth = powf(fdepth, 0.20f);

    int icidx = pnode->GetDecoIndex();

    if (balternate && icidx & 1) {
      float fg = color.GetY();
      float fb = color.GetZ();
      color.SetY(fb);
      color.SetZ(fg);
    }

    return (color * fdepth);
  }
  ///////////////////////////////////////////////////////////////////
  void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) {
    GedPrim prim;
    prim.ix1 = ix;
    prim.ix2 = ix + iw;
    prim.iy1 = iy;
    prim.iy2 = iy + ih;

		fvec4 uobj = mpCurrentGedVp->AssignPickId(pnode);

    if (mbPickMode) {
      AddToObjSet((void*)pnode);
      // printf( "insert obj<%p>\n", (void*) pnode );
    }

    prim._ucolor = mbPickMode ? uobj : GetStyleColor(pnode, ic); // Default Outline
    prim.meType = EPRIM_QUADS;
    prim.miSortKey = calcsort(isort);
    AddPrim(prim);
  }
  ///////////////////////////////////////////////////////////////////
  void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) {
    if (false == mbPickMode) {
      GedPrim prim;
      prim._ucolor = GetStyleColor(pnode, ic);
      prim.meType = EPRIM_LINES;
      prim.miSortKey = calcsort(isort + 1);

      prim.ix1 = ix;
      prim.ix2 = ix + iw;
      prim.iy1 = iy;
      prim.iy2 = iy;
      AddPrim(prim);

      prim.ix1 = ix + iw;
      prim.ix2 = ix + iw;
      prim.iy1 = iy;
      prim.iy2 = iy + ih;
      AddPrim(prim);

      prim.ix1 = ix + iw;
      prim.ix2 = ix;
      prim.iy1 = iy + ih;
      prim.iy2 = iy + ih;
      AddPrim(prim);

      prim.ix1 = ix;
      prim.ix2 = ix;
      prim.iy1 = iy + ih;
      prim.iy2 = iy;
      AddPrim(prim);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void DrawLine(GedObject* pnode, int ix, int iy, int ix2, int iy2, ESTYLE ic) {
    if (false == mbPickMode) {
      GedPrim prim;
      prim._ucolor = GetStyleColor(pnode, ic);
      prim.meType = EPRIM_LINES;
      prim.ix1 = ix;
      prim.ix2 = ix2;
      prim.iy1 = iy;
      prim.iy2 = iy2;
      prim.miSortKey = calcsort(4);
      AddPrim(prim);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih) {
    if (false == mbPickMode) {
      int ixi = ix + 2;
      int iyi = iy + 2;
      int ixi2 = ix + iw - 2;
      int iyi2 = iy + ih - 2;
      DrawLine(pnode, ixi, iyi, ixi2, iyi2, ESTYLE_DEFAULT_CHECKBOX);
      DrawLine(pnode, ixi2, iyi, ixi, iyi2, ESTYLE_DEFAULT_CHECKBOX);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void DrawDownArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic) {
    DrawLine(pnode, ix, iy, ix + iw, iy, GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix, iy, ix + (iw >> 1), iy + ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix + iw, iy, ix + (iw >> 1), iy + ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
  }
  ///////////////////////////////////////////////////////////////////
  void DrawRightArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic) {
    DrawLine(pnode, ix, iy, ix, iy + ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix, iy, ix + iw, iy + (ih >> 1), GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix, iy + ih, ix + iw, iy + (ih >> 1), GedSkin::ESTYLE_BUTTON_OUTLINE);
  }
  ///////////////////////////////////////////////////////////////////
  virtual void DrawText(GedObject* pnode, int ix, int iy, const char* ptext) {
    if (false == mbPickMode) {
      GedText text;
      text.ix = ix;
      text.iy = iy;
      text.mString = GedText::StringType(ptext);
      mTexts.push_back(text);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void Begin(GfxTarget* pTARG, GedVP* pVP) {
    mbPickMode = pTARG->FBI()->IsPickState();
    mpCurrentGedVp = pVP;
    mTexts.clear();
    clear();
    ClearObjSet();
  }
  ///////////////////////////////////////////////////////////////////
  void End(GfxTarget* pTARG) {
    int iw = mpCurrentGedVp->GetW();
    int ih = mpCurrentGedVp->GetH();
    lev2::GfxMaterialUI uimat(pTARG);
    miRejected = 0;
    miAccepted = 0;
    lev2::DynamicVertexBuffer<vtx_t>& VB = lev2::GfxEnv::GetSharedDynamicV16T16C16();
    ////////////////////////
    ork::fmtx4 mtxW = pTARG->MTXI()->RefMMatrix();
    pTARG->MTXI()->PushUIMatrix(iw, ih);
    pTARG->MTXI()->PushMMatrix(mtxW);
    ////////////////////////
    // pTARG->IMI()->QueFlush();
    for (PrimContainers::const_iterator itc = mPrimContainers.begin(); itc != mPrimContainers.end(); itc++) {
      const PrimContainer* primcontainer = itc->second;

      int inumlines = (int)primcontainer->mLinePrims.size();
      int inumquads = (int)primcontainer->mQuadPrims.size();
      int inumcusts = (int)primcontainer->mCustomPrims.size();

      uimat.SetUIColorMode(EUICOLOR_VTX);
      uimat._rasterstate.SetBlending(lev2::EBLENDING_OFF);

      const float fZ = 0.0f;

      ///////////////////////////////////////////////////////////
      // int ivbase = VB.GetNum();
      int icount = 0;
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(pTARG, &VB, inumquads * 6);
      {
        for (int i = 0; i < inumquads; i++) {
          const GedPrim* prim = primcontainer->mQuadPrims[i];
          if (IsVisible(pTARG, prim->iy1, prim->iy2)) {
            // printf( "DrawQuad ix1<%d> iy1<%d> ix2<%d> iy2<%d>\n", prim->ix1, prim->iy1, prim->ix2, prim->iy2 );

            vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy1, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy1, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy2, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy1, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy2, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy2, fZ), fvec4(), prim->_ucolor));
            icount += 6;
          }
        }
      }
      vw.UnLock(pTARG);
      pTARG->BindMaterial(&uimat);

      pTARG->GBI()->DrawPrimitive(vw, EPRIM_TRIANGLES);
      icount = 0;
      // ivbase += inumquads*6;
      ///////////////////////////////////////////////////////////
      if (false == mbPickMode) {
        for (int i = 0; i < inumcusts; i++) {
          const GedPrim* prim = primcontainer->mCustomPrims[i];
          if (IsVisible(pTARG, prim->iy1, prim->iy2)) {
            prim->mDrawCB(this, prim->mpNode, pTARG);
          }
        }
      }
      if (false == mbPickMode) {
        if (inumlines) {
          icount = 0;
          vw.Lock(pTARG, &VB, inumlines * 2);
          {
            for (int i = 0; i < inumlines; i++) {
              const GedPrim* prim = primcontainer->mLinePrims[i];
              if (IsVisible(pTARG, prim->iy1, prim->iy2)) {
                vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy1, fZ), fvec4(), prim->_ucolor));
                vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy2, fZ), fvec4(), prim->_ucolor));
                icount += 2;
              }
            }
          }
          vw.UnLock(pTARG);
          pTARG->BindMaterial(&uimat);
          if (icount)
            pTARG->GBI()->DrawPrimitive(vw, EPRIM_LINES);
        }
      }
    }
    ///////////////////////////////////////////////////////////
    ////////////////////////
    if (false == mbPickMode) { ////////////////////////
      uimat.SetUIColorMode(EUICOLOR_MOD);
      // pTARG->PushModColor(fcolor4(0.0f,0.0f,0.2f));
      pTARG->PushModColor(fcolor4::Black());
      lev2::FontMan::PushFont(mpFONT);
      lev2::FontMan::BeginTextBlock(pTARG);
      {
        int inumt = (int)mTexts.size();
        for (int it = 0; it < inumt; it++) {
          const GedText& text = mTexts[it];
          if (IsVisible(pTARG, text.iy, text.iy + 8)) {
            lev2::FontMan::DrawText(pTARG, text.ix, text.iy, text.mString.c_str());
          }
        }
      }
      lev2::FontMan::EndTextBlock(pTARG);
      lev2::FontMan::PopFont();
      pTARG->PopModColor();
    }
    ////////////////////////
    pTARG->MTXI()->PopMMatrix();
    pTARG->MTXI()->PopUIMatrix();
    pTARG->BindMaterial(0);
    mpCurrentGedVp = 0;
  }
};
///////////////////////////////////////////////////////////////////////////////
struct GedSkin1 : public GedSkin { ///////////////////////////////////////////////////////////////////
  bool mbPickMode;
  orkvector<GedText> mTexts;
  ///////////////////////////////////////////////////////////////////
  GedSkin1(GfxTarget* ptarg) : GedSkin(ptarg) {
    mpFONT = lev2::FontMan::GetFont("i14");
    miCHARW = mpFONT->GetFontDesc().miAdvanceWidth;
    miCHARH = mpFONT->GetFontDesc().miAdvanceHeight;
  }
  ///////////////////////////////////////////////////////////////////
  fvec4 GetStyleColor(GedObject* pnode, ESTYLE ic) {
    GedMapNode* pmapnode = rtti::autocast(pnode);

    bool bismapnode = (pmapnode != 0);

    fvec3 color;
    bool bsc = true;
    bool balternate = true;
    switch (ic) {
      case ESTYLE_BACKGROUND_1:
        color = bismapnode ? fvec3(0.2f, 0.2f, 0.3f) : fvec3(0.4f, 0.4f, 0.5f);
        break;
      case ESTYLE_BACKGROUND_2:
        color = fvec3(0.0f, 0.0f, 0.0f);
        break;
      case ESTYLE_BACKGROUND_3:
        color = fvec3(0.0f, 0.0f, 0.3f);
        break;
      case ESTYLE_BACKGROUND_4:
        color = fvec3(0.3f, 0.0f, 0.3f);
        break;
      case ESTYLE_BACKGROUND_5:
        color = fvec3(0.7f, 0.0f, 0.0f);
        break;
      case ESTYLE_BACKGROUND_OPS:
        color = fvec3(0.8f, 0.4f, 0.4f);
        balternate = false;
        break;
      case ESTYLE_BACKGROUND_OBJNODE_LABEL:
        color = fvec3(0.4f, 0.4f, 1.0f);
        balternate = false;
        break;
      case ESTYLE_BACKGROUND_GROUP_LABEL:
        color = fvec3(0.5f, 0.5f, 0.7f);
        balternate = false;
        break;
      case ESTYLE_BACKGROUND_MAPNODE_LABEL:
        color = fvec3(0.7f, 0.5f, 0.5f);
        balternate = false;
        break;
      case ESTYLE_DEFAULT_HIGHLIGHT:
        color = fvec3(0.1f, 0.2f, 0.3f);
        break;
      case ESTYLE_DEFAULT_OUTLINE:
        color = fvec3(0.7f, 0.7f, 0.7f);
        break;
      case ESTYLE_BUTTON_OUTLINE:
        color = fvec3(0.6f, 0.6f, 0.7f);
        balternate = false;
        bsc = false;
        break;
      case ESTYLE_DEFAULT_CHECKBOX:
        color = fvec3(0.6f, 0.6f, 0.7f);
        bsc = false;
        break;
      default:
        color = fvec3(0.0f, 0.0f, 0.0f);
        break;
    }

    float fdepth = bsc ? float(pnode->GetDepth()) / 12.0f : 1.0f;

    fdepth = powf(fdepth, 0.5f);

    int icidx = pnode->GetDecoIndex();

    if (balternate && icidx & 1) {
      float fg = color.GetY();
      float fb = color.GetZ();
      color.SetY(fb);
      color.SetZ(fg);
    }

    return (color * fdepth);
  }
  ///////////////////////////////////////////////////////////////////
  void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) {
    GedPrim prim;
    prim.ix1 = ix;
    prim.ix2 = ix + iw;
    prim.iy1 = iy;
    prim.iy2 = iy + ih;

		fvec4 uobj = mpCurrentGedVp->AssignPickId(pnode);

    if (mbPickMode) {
      AddToObjSet((void*)pnode);
      // printf( "insert obj<%p>\n", (void*) pnode );
    }

    prim._ucolor = mbPickMode ? uobj : GetStyleColor(pnode, ic); // Default Outline
    prim.meType = EPRIM_QUADS;
    prim.miSortKey = calcsort(isort);
    AddPrim(prim);
  }
  ///////////////////////////////////////////////////////////////////
  void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) {
    if (false == mbPickMode) {
      GedPrim prim;
      prim._ucolor = GetStyleColor(pnode, ic);
      prim.meType = EPRIM_LINES;
      prim.miSortKey = calcsort(isort + 1);

      prim.ix1 = ix;
      prim.ix2 = ix + iw;
      prim.iy1 = iy;
      prim.iy2 = iy;
      AddPrim(prim);

      prim.ix1 = ix + iw;
      prim.ix2 = ix + iw;
      prim.iy1 = iy;
      prim.iy2 = iy + ih;
      AddPrim(prim);

      prim.ix1 = ix + iw;
      prim.ix2 = ix;
      prim.iy1 = iy + ih;
      prim.iy2 = iy + ih;
      AddPrim(prim);

      prim.ix1 = ix;
      prim.ix2 = ix;
      prim.iy1 = iy + ih;
      prim.iy2 = iy;
      AddPrim(prim);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void DrawLine(GedObject* pnode, int ix, int iy, int ix2, int iy2, ESTYLE ic) {
    if (false == mbPickMode) {
      GedPrim prim;
      prim._ucolor = GetStyleColor(pnode, ic);
      prim.meType = EPRIM_LINES;
      prim.ix1 = ix;
      prim.ix2 = ix2;
      prim.iy1 = iy;
      prim.iy2 = iy2;
      prim.miSortKey = calcsort(4);
      AddPrim(prim);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih) {
    if (false == mbPickMode) {
      int ixi = ix + 2;
      int iyi = iy + 2;
      int ixi2 = ix + iw - 2;
      int iyi2 = iy + ih - 2;
      DrawLine(pnode, ixi, iyi, ixi2, iyi2, ESTYLE_DEFAULT_CHECKBOX);
      DrawLine(pnode, ixi2, iyi, ixi, iyi2, ESTYLE_DEFAULT_CHECKBOX);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void DrawDownArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic) {
    DrawLine(pnode, ix, iy, ix + iw, iy, GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix, iy, ix + (iw >> 1), iy + ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix + iw, iy, ix + (iw >> 1), iy + ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
  }
  ///////////////////////////////////////////////////////////////////
  void DrawRightArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic) {
    DrawLine(pnode, ix, iy, ix, iy + ih, GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix, iy, ix + iw, iy + (ih >> 1), GedSkin::ESTYLE_BUTTON_OUTLINE);
    DrawLine(pnode, ix, iy + ih, ix + iw, iy + (ih >> 1), GedSkin::ESTYLE_BUTTON_OUTLINE);
  }
  ///////////////////////////////////////////////////////////////////
  virtual void DrawText(GedObject* pnode, int ix, int iy, const char* ptext) {
    if (false == mbPickMode) {
      GedText text;
      text.ix = ix;
      text.iy = iy;
      text.mString = GedText::StringType(ptext);
      mTexts.push_back(text);
    }
  }
  ///////////////////////////////////////////////////////////////////
  void Begin(GfxTarget* pTARG, GedVP* pVP) {
    mbPickMode = pTARG->FBI()->IsPickState();
    mpCurrentGedVp = pVP;
    mTexts.clear();
    clear();
    ClearObjSet();
  }
  ///////////////////////////////////////////////////////////////////
  void End(GfxTarget* pTARG) {
    int iw = mpCurrentGedVp->GetW();
    int ih = mpCurrentGedVp->GetH();
    lev2::GfxMaterialUI uimat(pTARG);
    miRejected = 0;
    miAccepted = 0;
    lev2::DynamicVertexBuffer<vtx_t>& VB = lev2::GfxEnv::GetSharedDynamicV16T16C16();
    ////////////////////////
    const ork::fmtx4& mtxW = pTARG->MTXI()->RefMMatrix();
    pTARG->MTXI()->PushUIMatrix(iw, ih);
    pTARG->MTXI()->PushMMatrix(mtxW);
    ////////////////////////
    // pTARG->IMI()->QueFlush();
    for (PrimContainers::const_iterator itc = mPrimContainers.begin(); itc != mPrimContainers.end(); itc++) {
      const PrimContainer* primcontainer = itc->second;

      int inumlines = (int)primcontainer->mLinePrims.size();
      int inumquads = (int)primcontainer->mQuadPrims.size();
      int inumcusts = (int)primcontainer->mCustomPrims.size();

      uimat.SetUIColorMode(EUICOLOR_VTX);
      uimat._rasterstate.SetBlending(lev2::EBLENDING_OFF);

      const float fZ = 0.0f;

      ///////////////////////////////////////////////////////////
      // int ivbase = VB.GetNum();

      int icount = 0;
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(pTARG, &VB, inumquads * 6);
      {
        for (int i = 0; i < inumquads; i++) {
          const GedPrim* prim = primcontainer->mQuadPrims[i];
          if (IsVisible(pTARG, prim->iy1, prim->iy2)) {
            vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy1, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy1, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy2, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy1, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy2, fZ), fvec4(), prim->_ucolor));
            vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy2, fZ), fvec4(), prim->_ucolor));
            icount += 6;
          }
        }
      }
      vw.UnLock(pTARG);

      pTARG->BindMaterial(&uimat);

      pTARG->GBI()->DrawPrimitive(vw, EPRIM_TRIANGLES);
      icount = 0;
      // ivbase += inumquads*6;
      ///////////////////////////////////////////////////////////
      if (false == mbPickMode) {
        for (int i = 0; i < inumcusts; i++) {
          const GedPrim* prim = primcontainer->mCustomPrims[i];
          if (IsVisible(pTARG, prim->iy1, prim->iy2)) {
            prim->mDrawCB(this, prim->mpNode, pTARG);
          }
        }
      }
      if (false == mbPickMode) {
        if (inumlines) {
          icount = 0;
          vw.Lock(pTARG, &VB, inumlines * 2);
          {
            for (int i = 0; i < inumlines; i++) {
              const GedPrim* prim = primcontainer->mLinePrims[i];
              if (IsVisible(pTARG, prim->iy1, prim->iy2)) {
                vw.AddVertex(vtx_t(fvec4(prim->ix1, prim->iy1, fZ), fvec4(), prim->_ucolor));
                vw.AddVertex(vtx_t(fvec4(prim->ix2, prim->iy2, fZ), fvec4(), prim->_ucolor));
                icount += 2;
              }
            }
          }
          vw.UnLock(pTARG);
          pTARG->BindMaterial(&uimat);
          if (icount)
            pTARG->GBI()->DrawPrimitive(vw, EPRIM_LINES);
        }
      }
    }
    ///////////////////////////////////////////////////////////
    ////////////////////////
    if (false == mbPickMode) { ////////////////////////
      uimat.SetUIColorMode(EUICOLOR_MOD);
      lev2::FontMan::PushFont(mpFONT);
      lev2::FontMan::BeginTextBlock(pTARG);
      pTARG->PushModColor(fcolor4(0.8f, 0.9f, 1.0f));
      {
        int inumt = (int)mTexts.size();
        for (int it = 0; it < inumt; it++) {
          const GedText& text = mTexts[it];
          if (IsVisible(pTARG, text.iy, text.iy + 8)) {
            lev2::FontMan::DrawText(pTARG, text.ix, text.iy, text.mString.c_str());
          }
        }
      }
      lev2::FontMan::EndTextBlock(pTARG);
      lev2::FontMan::PopFont();
      pTARG->PopModColor();
    }
    ////////////////////////
    pTARG->MTXI()->PopMMatrix();
    pTARG->MTXI()->PopUIMatrix();
    pTARG->BindMaterial(0);
    mpCurrentGedVp = 0;
  }
};
orkvector<GedSkin*> InstantiateSkins() {
  while (0 == lev2::GfxEnv::GetRef().GetLoaderTarget()) {
    ork::msleep(100);
  }
  auto targ = lev2::GfxEnv::GetRef().GetLoaderTarget();

  orkvector<GedSkin*> skins;
  skins.push_back(new GedSkin0(targ));
  skins.push_back(new GedSkin1(targ));
  return skins;
}
//////////////////////////////////////////////////////////////////////////////
}}} // namespace ork::tool::ged
