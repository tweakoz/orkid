////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

  int _3d_char_width    = 0;
  int _3d_char_height   = 0;
  int _3d_char_u_offset = 0;
  int _3d_char_v_offset = 0;
  int _3d_char_u_width  = 0;
  int _3d_char_v_height = 0;

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

struct Font {
  /////////////////////////////////////////////
public:
  /////////////////////////////////////////////

  static const int kMaxChars;
  U8 muaCurColor[4];

  Font(const std::string& fontname, const std::string& filename);

  void LoadFromDisk(Context* pTARG, const FontDesc& fd);
  const FontDesc& GetFontDesc() const;
  GfxMaterial* material() const;

  void enqueueCharacter(VtxWriter<SVtxV12C4T16>& vw, float fx, float fy, int iu, int iv, U32 ucolor) const;
  const FontDesc& description() const;

  std::string msFileName;
  std::string msFontName;
  GfxMaterial* mpMaterial;
  freestyle_mtl_ptr_t _fs_material;
  fxtechnique_constptr_t _tek_stereo_text;
  fxpipeline_ptr_t _pipe_stereo;
  pbrmaterial_ptr_t _materialDeferred;
  mutable bool _use_deferred = false;
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

using textitem_vect = std::vector<TextItem>;

///////////////////////////////////////////////////////////////////////////////

struct TextBlockState {
  size_t _maxcharcount = 0;
  const Font* _font    = nullptr;
  std::stack<const Font*> _fontstack;
  bool _stereo_3d_text = false;
  rcid_ptr_t _overrideRCID;
};

using textblockstate_ptr_t = std::shared_ptr<TextBlockState>;

///////////////////////////////////////////////////////////////////////////////

struct FontMan { //: public NoRttiSingleton<FontMan> {
  /////////////////////////////////////////////

  using vtxwriter_t      = VtxWriter<SVtxV12C4T16>;
  using vtxwriter_ptr_t  = std::shared_ptr<vtxwriter_t>;
  using vtxwriter_vect_t = std::vector<vtxwriter_ptr_t>;

  static FontMan* instance();
  static FontMan& GetRef();

  ~FontMan();

  //////////////////////////////////////////////////////

  void _addFont(Context* pTARG, const FontDesc& fdesc);
  void _gpuInit(Context* pTARG);

  void _beginTextBlockWithState(Context* pTARG, textblockstate_ptr_t tbstate);
  void _endTextBlockWithState(Context* pTARG, textblockstate_ptr_t tbstate);

  void _beginTextBlock(Context* pTARG, int imaxcharcount = 0);
  void _endTextBlock(Context* pTARG);
  void _enqueueText(float fx, float fy, vtxwriter_t& vwriter, const fixedstring_t& text, const fvec4& color);

  vtxwriter_t& textwriter() {
    return mTextWriter;
  }
  textblockstate_ptr_t _topstate();

  //////////////////////////////////////////////////////

  static int stringWidth(int numchars);

  static void gpuInit(Context* pTARG);

  static void DrawText(Context* pTARG, int iX, int iY, const char* pFmt, ...);
  static void DrawCenteredText(Context* pTARG, int iY, const char* pFmt, ...);

  static void DrawTextItems(Context* pTARG, const textitem_vect& items);

  /////////////////////////////////////////////
  // Font Management
  /////////////////////////////////////////////

  void _bindFont(const Font* pFont);
  const Font* _pushFont(const std::string& name);
  const Font* _popFont();

  static const Font* GetFont(const std::string& name);
  static const Font* currentFont();
  static const Font* SetCurrentFont(const std::string& name);
  static void PushFont(const Font* pFont);
  static const Font* PushFont(const std::string& name);
  static const Font* PopFont();
  static void beginTextBlock(Context* pTARG, int imaxcharcount = 0);
  static void endTextBlock(Context* pTARG);

  /////////////////////////////////////////////

protected:
  /////////////////////////////////////////////

  orkvector<Font*> mFontVect;
  orkmap<std::string, Font*> mFontMap;
  Font* mpDefaultFont;
  vtxwriter_t mTextWriter;
  CharDesc mCharDescriptions[256];
  vtxwriter_vect_t _writers;
  bool _doGpuInit = true;
  textblockstate_ptr_t _defaultTextBlockState;
  std::stack<textblockstate_ptr_t> _textBlockStateStack;
  textblockstate_ptr_t _currentTextBlockState;

  FontMan();

private:
  FontMan(const FontMan& other) = delete;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
