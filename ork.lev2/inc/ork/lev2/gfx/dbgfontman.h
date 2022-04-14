////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "gfxenv.h"
#include <ork/kernel/core/singleton.h>
#include <ork/util/stl_ext.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

struct FontDesc {
  std::string mFontName;
  std::string mFontFile;

  int stringWidth(int numchars) const;

  int miTexWidth;
  int miTexHeight;
  int miCellWidth;
  int miCellHeight;
  int miCharWidth;
  int miCharHeight;
  int miCharOffsetX;
  int miCharOffsetY;
  int miYShift;
  int miAdvanceWidth;
  int miAdvanceHeight;

  FontDesc()
      : miCellWidth(0)
      , miCellHeight(0)
      , miCharWidth(0)
      , miCharHeight(0)
      , miCharOffsetX(0)
      , miCharOffsetY(0)
      , miYShift(0)
      , miAdvanceWidth(0)
      , miAdvanceHeight(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct CharDesc {
  char ch;
  int miRow;
  int miCol;

  CharDesc()
      : ch(0)
      , miRow(0)
      , miCol(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class Font {
  /////////////////////////////////////////////
public:
  /////////////////////////////////////////////

  static const int kMaxChars;
  U8 muaCurColor[4];

  Font(const std::string& fontname, const std::string& filename);

  void LoadFromDisk(Context* pTARG, const FontDesc& fd);
  const FontDesc& GetFontDesc(void) {
    return mFontDesc;
  }
  GfxMaterial* GetMaterial();

  void enqueueCharacter(VtxWriter<SVtxV12C4T16>& vw, float fx, float fy, int iu, int iv, U32 ucolor);
  const FontDesc& description() const {
    return mFontDesc;
  }

  std::string msFileName;
  std::string msFontName;
  GfxMaterial* mpMaterial;
  pbrmaterial_ptr_t _materialDeferred;
  bool _use_deferred = false;
  texture_ptr_t _texture;
  FontDesc mFontDesc;
};

///////////////////////////////////////////////////////////////////////////////

static constexpr size_t KFIXEDSTRINGLEN = 1024;
using fixedstring_t                     = FixedString<KFIXEDSTRINGLEN>;

struct TextItem {
  fmtx4 _wmatrix;
  fixedstring_t _text;
};

///////////////////////////////////////////////////////////////////////////////

using textitem_vect = std::vector<TextItem>;

///////////////////////////////////////////////////////////////////////////////

struct FontMan { //: public NoRttiSingleton<FontMan> {
  /////////////////////////////////////////////

  using vtxwriter_t = VtxWriter<SVtxV12C4T16>;
  using vtxwriter_ptr_t = std::shared_ptr<vtxwriter_t>;
  using vtxwriter_vect_t = std::vector<vtxwriter_ptr_t>;

  static FontMan* instance();
  static FontMan& GetRef();

  ~FontMan();

  //////////////////////////////////////////////////////

  void _addFont(Context* pTARG, const FontDesc& fdesc);
  void _gpuInit(Context* pTARG);
  void _bindFont(Font* pFont) {
    OrkAssert(pFont);
    mpCurrentFont = pFont;
  }
  Font* _pushFont(const std::string& name) {
    Font* pFont = OldStlSchoolFindValFromKey(mFontMap, name, (Font*)nullptr);
    OrkAssert(pFont);
    mFontStack.push(mpCurrentFont);
    mpCurrentFont = pFont;
    return pFont;
  }

  void _beginTextBlock(Context* pTARG, int imaxcharcount = 0);
  void _endTextBlock(Context* pTARG);
  void _enqueueText(float fx, float fy, vtxwriter_t& vwriter, const fixedstring_t& text, const fvec4& color);

  //////////////////////////////////////////////////////

  static int stringWidth(int numchars);

  static void gpuInit(Context* pTARG);

  static void DrawText(Context* pTARG, int iX, int iY, const char* pFmt, ...);
  static void DrawCenteredText(Context* pTARG, int iY, const char* pFmt, ...);

  static void DrawTextItems( Context* pTARG, const textitem_vect& items );

  static Font* currentFont(void) {
    return GetRef().mpCurrentFont;
  }

  static Font* GetFont(const std::string& name) {
    Font* pFont = OldStlSchoolFindValFromKey(GetRef().mFontMap, name, (Font*)0);
    return pFont;
  }

  static Font* SetCurrentFont(const std::string& name) {
    Font* pFont = OldStlSchoolFindValFromKey(GetRef().mFontMap, name, (Font*)0);
    OrkAssert(pFont);
    GetRef().mpCurrentFont = pFont;
    return pFont;
  }
  static void PushFont(Font* pFont) {
    OrkAssert(pFont);
    GetRef().mFontStack.push(GetRef().mpCurrentFont);
    GetRef().mpCurrentFont = pFont;
  }
  static Font* PushFont(const std::string& name) {
    return instance()->_pushFont(name);
  }
  static Font* PopFont() {
    GetRef().mFontStack.pop();
    Font* pFont = GetRef().mFontStack.top();
    OrkAssert(pFont);
    GetRef().mpCurrentFont = pFont;
    return pFont;
  }

  static Font* SetDefaultFont(void) {
    GetRef().mpCurrentFont = GetRef().mpDefaultFont;
    return GetRef().mpCurrentFont;
  }

  static void beginTextBlock(Context* pTARG, int imaxcharcount = 0);
  static void endTextBlock(Context* pTARG);

  /////////////////////////////////////////////
protected:
  /////////////////////////////////////////////

  orkstack<Font*> mFontStack;
  orkvector<Font*> mFontVect;
  orkmap<std::string, Font*> mFontMap;
  Font* mpCurrentFont;
  Font* mpDefaultFont;
  vtxwriter_t mTextWriter;
  CharDesc mCharDescriptions[256];
  vtxwriter_vect_t _writers;
  bool _doGpuInit = true;

  FontMan();

private:
  FontMan(const FontMan& other) = delete;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
