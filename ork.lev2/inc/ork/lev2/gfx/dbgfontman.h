////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
  GfxMaterial* GetMaterial(void) {
    return mpMaterial;
  }
  void QueChar(Context* pTarg, VtxWriter<SVtxV12C4T16>& vw, int ix, int iy, int iu, int iv, U32 ucolor);

  std::string msFileName;
  std::string msFontName;
  GfxMaterial* mpMaterial;
  FontDesc mFontDesc;
};

///////////////////////////////////////////////////////////////////////////////

struct FontMan { //: public NoRttiSingleton<FontMan> {
  /////////////////////////////////////////////

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
  static constexpr size_t KFIXEDSTRINGLEN = 1024;
  using fixedstring_t                     = FixedString<KFIXEDSTRINGLEN>;

  void _beginTextBlock(Context* pTARG, int imaxcharcount = 0);
  void _endTextBlock(Context* pTARG);
  void _drawText(Context* pTARG, int iX, int iY, const fixedstring_t& text);

  //////////////////////////////////////////////////////

  static void gpuInit(Context* pTARG);

  static void DrawText(Context* pTARG, int iX, int iY, const char* pFmt, ...);

  static Font* GetCurrentFont(void) {
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

  static void BeginTextBlock(Context* pTARG, int imaxcharcount = 0);
  static void EndTextBlock(Context* pTARG);

  /////////////////////////////////////////////
protected:
  /////////////////////////////////////////////

  orkstack<Font*> mFontStack;
  orkvector<Font*> mFontVect;
  orkmap<std::string, Font*> mFontMap;
  Font* mpCurrentFont;
  Font* mpDefaultFont;
  VtxWriter<SVtxV12C4T16> mTextWriter;
  CharDesc mCharDescriptions[256];
  bool _doGpuInit = true;

  FontMan();

private:
  FontMan(const FontMan& other) = delete;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
