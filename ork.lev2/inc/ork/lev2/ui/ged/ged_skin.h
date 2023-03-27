#pragma once

#include "ged.h"
#include <ork/lev2/lev2_types.h>

namespace ork::lev2::ged {

class GedSkin {

public:
  typedef void (*DrawCB)(GedSkin* pskin, GedObject* pnode, ork::lev2::Context* pTARG);

  void gpuInit(lev2::Context* ctx);

  ork::lev2::freestyle_mtl_ptr_t _material;
  const ork::lev2::FxShaderTechnique* _tekpick     = nullptr;
  const ork::lev2::FxShaderTechnique* _tekvtxcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tekvtxpick  = nullptr;
  const ork::lev2::FxShaderTechnique* _tekmodcolor = nullptr;
  const ork::lev2::FxShaderParam* _parmvp          = nullptr;
  const ork::lev2::FxShaderParam* _parmodcolor     = nullptr;
  const ork::lev2::FxShaderParam* _parobjid        = nullptr;

  struct GedPrim {
    DrawCB mDrawCB;
    GedObject* mpNode;
    int ix1, iy1, ix2, iy2;
    fvec4 _ucolor;
    ork::lev2::PrimitiveType meType;
    int miSortKey;

    GedPrim()
        : mDrawCB(0)
        , mpNode(0)
        , ix1(0)
        , ix2(0)
        , iy1(0)
        , iy2(0)
        , _ucolor(0)
        , miSortKey(0)
        , meType(ork::lev2::PrimitiveType::END) {
    }
  };

  struct PrimContainer {
    static const int kmaxprims    = 8192;
    static const int kmaxprimsper = 4096;
    fixed_pool<GedPrim, kmaxprims> mPrimPool;
    fixedvector<GedPrim*, kmaxprimsper> mLinePrims;
    fixedvector<GedPrim*, kmaxprimsper> mQuadPrims;
    fixedvector<GedPrim*, kmaxprimsper> mCustomPrims;

    void clear();
  };

  GedSkin();

  typedef enum {
    ESTYLE_BACKGROUND_1 = 0,
    ESTYLE_BACKGROUND_2,
    ESTYLE_BACKGROUND_3,
    ESTYLE_BACKGROUND_4,
    ESTYLE_BACKGROUND_5,
    ESTYLE_BACKGROUND_OPS,
    ESTYLE_BACKGROUND_GROUP_LABEL,
    ESTYLE_BACKGROUND_MAPNODE_LABEL,
    ESTYLE_BACKGROUND_OBJNODE_LABEL,
    ESTYLE_DEFAULT_HIGHLIGHT,
    ESTYLE_DEFAULT_OUTLINE,
    ESTYLE_DEFAULT_CAPTION,
    ESTYLE_DEFAULT_CHECKBOX,
    ESTYLE_BUTTON_OUTLINE,
  } ESTYLE;

  virtual void Begin(ork::lev2::Context* pTARG, GedSurface* pgedvp)                                       = 0;
  virtual void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort = 0)      = 0;
  virtual void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort = 0) = 0;
  virtual void DrawLine(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                      = 0;
  virtual void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih)                             = 0;
  virtual void DrawDownArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                 = 0;
  virtual void DrawRightArrow(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                = 0;
  virtual void DrawText(GedObject* pnode, int ix, int iy, const char* ptext)                              = 0;
  virtual void End(ork::lev2::Context* pTARG)                                                             = 0;

  void SetScrollY(int iscrolly) {
    miScrollY = iscrolly;
  }

  void AddPrim(const GedPrim& cb); // { mPrims.AddSorted(calcsort(cb.miSortKey),cb); }

  int GetScrollY() const {
    return miScrollY;
  }

  int calcsort(int isort) {
    int ioutsort = (isort << 16); //+int(mPrims.size());
    return ioutsort;
  }

  static const int kMaxPrimContainers = 32;
  fixed_pool<PrimContainer, 32> mPrimContainerPool;
  typedef fixedlut<int, PrimContainer*, kMaxPrimContainers> PrimContainers;

  void clear();

  int char_w() const {
    return miCHARW;
  }
  int char_h() const {
    return miCHARH;
  }

protected:
  PrimContainers mPrimContainers;

  int miScrollY;
  int miRejected;
  int miAccepted;
  GedSurface* mpCurrentGedVp;
  ork::lev2::Font* mpFONT;
  int miCHARW;
  int miCHARH;

  bool IsVisible(ork::lev2::Context* pTARG, int iy1, int iy2) {
    int iry1 = iy1 + miScrollY;
    int iry2 = iy2 + miScrollY;
    int ih   = pTARG->mainSurfaceHeight();

    if (iry2 < 0) {
      miRejected++;
      return false;
    } else if (iry1 > ih) {
      miRejected++;
      return false;
    }

    miAccepted++;
    return true;
  }
};

} //namespace ork::lev2::ged {
