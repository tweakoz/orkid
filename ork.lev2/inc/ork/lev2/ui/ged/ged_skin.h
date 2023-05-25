#pragma once

#include "ged.h"
#include <ork/lev2/lev2_types.h>
#include <ork/kernel/Array.h>
#include <ork/kernel/string/ArrayString.h>

namespace ork::lev2::ged {

struct GedSkin {

public:

  using vtx_t = SVtxV16T16C16 ;

  typedef void (*DrawCB)(GedSkin* pskin, GedObject* pnode, ork::lev2::Context* pTARG);
  using prim_lambda_t = std::function<void()>;
  void gpuInit(lev2::Context* ctx);

  ork::lev2::freestyle_mtl_ptr_t _material;
  const ork::lev2::FxShaderTechnique* _tekpick     = nullptr;
  const ork::lev2::FxShaderTechnique* _tekvtxcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tektexcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tekvtxpick  = nullptr;
  const ork::lev2::FxShaderTechnique* _tekmodcolor = nullptr;
  const ork::lev2::FxShaderTechnique* _tekcolorwheel = nullptr;
  const ork::lev2::FxShaderParam* _parmvp          = nullptr;
  const ork::lev2::FxShaderParam* _parmodcolor     = nullptr;
  const ork::lev2::FxShaderParam* _partexture     = nullptr;
  const ork::lev2::FxShaderParam* _parobjid        = nullptr;
  const ork::lev2::FxShaderParam* _partime        = nullptr;

  struct GedText {
    using StringType = ork::ArrayString<128>;
    int ix = 0;
    int iy = 0;
    StringType mString;
  };

  struct GedPrim {
    prim_lambda_t _renderLambda;
    GedObject* mpNode = nullptr;
    int ix1 = 0;
    int iy1 = 0;
    int ix2 = 0;
    int iy2 = 0;
    fvec4 _ucolor;
    texture_ptr_t _texture;
    ork::lev2::PrimitiveType meType = ork::lev2::PrimitiveType::END;
    int miSortKey = 0;
  };

  struct PrimContainer {
    static const int kmaxprims    = 8192;
    static const int kmaxprimsper = 4096;
    fixed_pool<GedPrim, kmaxprims> mPrimPool;
    fixedvector<GedPrim*, kmaxprimsper> mLinePrims;
    fixedvector<GedPrim*, kmaxprimsper> mQuadPrims;
    std::unordered_map<ork::lev2::texture_ptr_t,fixedvector<GedPrim*, kmaxprimsper>> mTexQuadPrims;
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
    ESTYLE_CUSTOM_COLOR,
    ESTYLE_DONT_CARE,
  } ESTYLE;

  void DrawColorBox(GedObject* pnode, int ix, int iy, int iw, int ih, fvec4 color, int isort = 0);

  virtual void Begin(ork::lev2::Context* pTARG, GedSurface* pgedvp)                                       = 0;
  virtual void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort = 0)      = 0;
  virtual void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort = 0) = 0;
  virtual void DrawLine(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic)                      = 0;
  virtual void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih)                             = 0;
  virtual void DrawDownArrow(GedObject* pnode, int ix, int iy, ESTYLE ic)                 = 0;
  virtual void DrawRightArrow(GedObject* pnode, int ix, int iy, ESTYLE ic)                = 0;
  virtual void DrawUpArrow(GedObject* pnode, int ix, int iy, ESTYLE ic)                = 0;
  virtual void DrawText(GedObject* pnode, int ix, int iy, const char* ptext)                              = 0;
  virtual void End(ork::lev2::Context* pTARG)                                                             = 0;

  void AddPrim(const GedPrim& cb); // { mPrims.AddSorted(calcsort(cb.miSortKey),cb); }

  int calcsort(int isort) {
    int ioutsort = (isort << 16); //+int(mPrims.size());
    return ioutsort;
  }

  static const int kMaxPrimContainers = 32;
  fixed_pool<PrimContainer, 32> mPrimContainerPool;
  typedef fixedlut<int, PrimContainer*, kMaxPrimContainers> PrimContainers;

  void clear();

  int char_w() const {
    return _char_w;
  }
  int char_h() const {
    return _char_h;
  }

  bool IsVisible(ork::lev2::Context* pTARG, int iy1, int iy2) {
    int iry1 = iy1 + _scrollY;
    int iry2 = iy2 + _scrollY;
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
  static std::set<void*>& GetObjSet();
  static void ClearObjSet();
  static void AddToObjSet(void* pobj);
  static bool IsObjInSet(void* pobj);

  PrimContainers mPrimContainers;

  void pushCustomColor(fcolor3 color);
  void popCustomColor();

  void DrawTexBox( GedObject* pnode, int ix, int iy, texture_ptr_t tex, fvec4 color );

  int _scrollY = 0;
  int miRejected = 0;
  int miAccepted = 0;
  GedSurface* _gedVP = nullptr;
  ork::lev2::Font* _font = nullptr;
  int _char_w = 0;
  int _char_h = 0;
  int _bannerHeight = 0;
  orkvector<GedText> _text_vect;
  bool _is_pickmode = false;
  fmtx4 _uiMatrix;
  fmtx4 _mMatrix;
  fmtx4 _uiMVPMatrix;
  std::stack<fcolor3> _colorStack;
  ork::Timer _timer;
  std::unordered_map<uint64_t, texture_ptr_t> _textures;
};

struct GedSkin0 : public GedSkin {

  GedSkin0(ork::lev2::Context* ctx);
  fvec4 GetStyleColor(GedObject* pnode, ESTYLE ic);
  void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) final;
  void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) final;
  void DrawLine(GedObject* pnode, int ix, int iy, int ix2, int iy2, ESTYLE ic) final;
  void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih) final;
  void DrawDownArrow(GedObject* pnode, int ix, int iy, ESTYLE ic) final;
  void DrawRightArrow(GedObject* pnode, int ix, int iy, ESTYLE ic) final;
  void DrawUpArrow(GedObject* pnode, int ix, int iy, ESTYLE ic) final;
  void DrawText(GedObject* pnode, int ix, int iy, const char* ptext) final;
  void Begin(Context* pTARG, GedSurface* pVP) final;
  void End(Context* pTARG) final;

};

struct GedSkin1 : public GedSkin {

  GedSkin1(ork::lev2::Context* ctx);
  fvec4 GetStyleColor(GedObject* pnode, ESTYLE ic);
  void DrawBgBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) final;
  void DrawOutlineBox(GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort) final;
  void DrawLine(GedObject* pnode, int ix, int iy, int ix2, int iy2, ESTYLE ic) final;
  void DrawCheckBox(GedObject* pnode, int ix, int iy, int iw, int ih) final;
  void DrawDownArrow(GedObject* pnode, int ix, int iy, ESTYLE ic) final;
  void DrawRightArrow(GedObject* pnode, int ix, int iy, ESTYLE ic) final;
  void DrawUpArrow(GedObject* pnode, int ix, int iy, ESTYLE ic) final;
  void DrawText(GedObject* pnode, int ix, int iy, const char* ptext) final;
  void Begin(Context* pTARG, GedSurface* pVP) final;
  void End(Context* pTARG) final;

};

} //namespace ork::lev2::ged {
