////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
      : miCellWidth(0), miCellHeight(0), miCharWidth(0), miCharHeight(0), miCharOffsetX(0), miCharOffsetY(0), miYShift(0),
        miAdvanceWidth(0), miAdvanceHeight(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct CharDesc {
  char ch;
  int miRow;
  int miCol;

  CharDesc() : ch(0), miRow(0), miCol(0) {}
};

///////////////////////////////////////////////////////////////////////////////

class Font {
  /////////////////////////////////////////////
public:
  /////////////////////////////////////////////

  static const int kMaxChars;
  U8 muaCurColor[4];

  Font(const std::string& fontname, const std::string& filename);

  void LoadFromDisk(GfxTarget* pTARG, const FontDesc& fd);
  const FontDesc& GetFontDesc(void) { return mFontDesc; }
  GfxMaterial* GetMaterial(void) { return mpMaterial; }
  void QueChar(GfxTarget* pTarg, VtxWriter<SVtxV12C4T16>& vw, int ix, int iy, int iu, int iv, U32 ucolor);

  /////////////////////////////////////////////
private:
  /////////////////////////////////////////////

  std::string msFileName;
  std::string msFontName;
  GfxMaterial* mpMaterial;
  FontDesc mFontDesc;
};

///////////////////////////////////////////////////////////////////////////////

class FontMan : public NoRttiSingleton<FontMan> {
  /////////////////////////////////////////////
public: //
  /////////////////////////////////////////////

  FontMan();
  ~FontMan();

  static void InitFonts(GfxTarget* pTARG);

  static void AddFont(GfxTarget* pTARG, const FontDesc& fdesc);
  static void DrawText(GfxTarget* pTARG, int iX, int iY, const char* pFmt, ...);

  static Font* GetCurrentFont(void) { return GetRef().mpCurrentFont; }

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
    Font* pFont = OldStlSchoolFindValFromKey(GetRef().mFontMap, name, (Font*)0);
    OrkAssert(pFont);
    GetRef().mFontStack.push(GetRef().mpCurrentFont);
    GetRef().mpCurrentFont = pFont;
    return pFont;
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

  static void BeginTextBlock(GfxTarget* pTARG, int imaxcharcount = 0);
  static void EndTextBlock(GfxTarget* pTARG);

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
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
