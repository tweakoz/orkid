////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/targetinterfaces.h>

namespace ork { namespace lev2 {

const int Font::kMaxChars = 16384;

FontMan* FontMan::instance() {
  struct PublicFontMan : public FontMan {};
  static std::shared_ptr<PublicFontMan> _instance = std::make_shared<PublicFontMan>();
  // printf("FontMan::instance<%p>\n", _instance.get());
  return _instance.get();
}
FontMan& FontMan::GetRef() {
  return *instance();
}
///////////////////////////////////////////////////////////////////////////////

FontMan::FontMan()
    : mpCurrentFont(nullptr) {

  for (int ich = 0; ich <= 255; ich++) {
    CharDesc& desc = mCharDescriptions[ich];
    desc.miRow     = ich >> 4;
    desc.miCol     = ich % 16;
    desc.ch        = (char)ich;
  }
}
FontMan::~FontMan() {
  for (auto item : GetRef().mFontVect)
    delete item;
  GetRef().mFontMap.clear();
}

///////////////////////////////////////////////////////////////////////////////

int FontDesc::stringWidth(int numchars) const {
  return miAdvanceWidth * numchars;
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_beginTextBlock(Context* pTARG, int imaxcharcount) {
  _gpuInit(pTARG);

  if (0 == imaxcharcount)
    imaxcharcount = 2048;
  int inumv = imaxcharcount * 8;

  VertexBufferBase& VB = pTARG->IMI()->RefTextVB();
  OrkAssert(false == VB.IsLocked());
  VtxWriter<SVtxV12C4T16>& vw = mTextWriter;
  new (&vw) VtxWriter<SVtxV12C4T16>;
  vw.miWriteCounter = 0;
  vw.miWriteBase    = 0;
  int inuminvb      = VB.GetNumVertices();
  int imaxinvb      = VB.GetMax();
  // printf( "inuminvb<%d> imaxinvb<%d> inumv<%d>\n", inuminvb, imaxinvb, inumv );
  if ((inuminvb + inumv) >= imaxinvb) {
    VB.Reset();
    VB.SetNumVertices(0);
  }
  vw.Lock(pTARG, &pTARG->IMI()->RefTextVB(), inumv);
}
void FontMan::_endTextBlock(Context* pTARG) {
  OrkAssert(pTARG->IMI()->RefTextVB().IsLocked());
  mTextWriter.UnLock(pTARG);
  pTARG->BindMaterial(mpCurrentFont->GetMaterial());
  bool bdraw = mTextWriter.miWriteCounter != 0;
  if (bdraw) {
    pTARG->GBI()->DrawPrimitive(mTextWriter, ork::lev2::EPrimitiveType::TRIANGLES);
  }
  pTARG->BindMaterial(0);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::BeginTextBlock(Context* pTARG, int imaxcharcount) {
  instance()->_beginTextBlock(pTARG, imaxcharcount);
}

void FontMan::EndTextBlock(Context* pTARG) {
  instance()->_endTextBlock(pTARG);
}

// Inconsolata font from http://www.levien.com/type/myfonts/inconsolata.html
// font textures built with F2IBuilder http://sourceforge.net/projects/f2ibuilder/
//  set texture size to 512
//  use metrics off
//  use a monospace font
//  show grid should be fine so long as the font stays away from the cell boundaries
//  a 'cell' is one of the 256(16x16) tiles in the texture,
//   if the texture size is 512x512, then a single cell will be 32x32

void FontMan::gpuInit(Context* pTARG) {
  instance()->_gpuInit(pTARG);
}

void FontMan::_gpuInit(Context* pTARG) {
  FontDesc Inconsolata12;
  Inconsolata12.mFontName       = "i12";
  Inconsolata12.mFontFile       = "lev2://textures/Inconsolata12";
  Inconsolata12.miTexWidth      = 512;
  Inconsolata12.miTexHeight     = 512;
  Inconsolata12.miCellWidth     = (512 / 16);
  Inconsolata12.miCellHeight    = (512 / 16);
  Inconsolata12.miCharWidth     = 12;
  Inconsolata12.miCharHeight    = 12;
  Inconsolata12.miCharOffsetX   = 12;
  Inconsolata12.miCharOffsetY   = 12;
  Inconsolata12.miAdvanceWidth  = 6;
  Inconsolata12.miAdvanceHeight = 12;

  FontDesc Inconsolata13;
  Inconsolata13.mFontName       = "i13";
  Inconsolata13.mFontFile       = "lev2://textures/Inconsolata13";
  Inconsolata13.miTexWidth      = 512;
  Inconsolata13.miTexHeight     = 512;
  Inconsolata13.miCellWidth     = (512 / 16);
  Inconsolata13.miCellHeight    = (512 / 16);
  Inconsolata13.miCharWidth     = 13;
  Inconsolata13.miCharHeight    = 13;
  Inconsolata13.miCharOffsetX   = 11;
  Inconsolata13.miCharOffsetY   = 11;
  Inconsolata13.miAdvanceWidth  = 7;
  Inconsolata13.miAdvanceHeight = 13;
  Inconsolata13.miYShift        = -1;

  FontDesc Inconsolata14;
  Inconsolata14.mFontName       = "i14";
  Inconsolata14.mFontFile       = "lev2://textures/Inconsolata14";
  Inconsolata14.miTexWidth      = 512;
  Inconsolata14.miTexHeight     = 512;
  Inconsolata14.miCellWidth     = (512 / 16);
  Inconsolata14.miCellHeight    = (512 / 16);
  Inconsolata14.miCharWidth     = 14;
  Inconsolata14.miCharHeight    = 24;
  Inconsolata14.miCharOffsetX   = 12;
  Inconsolata14.miCharOffsetY   = 8;
  Inconsolata14.miYShift        = -1;
  Inconsolata14.miAdvanceWidth  = 7;
  Inconsolata14.miAdvanceHeight = 12;

  FontDesc Inconsolata16;
  Inconsolata16.mFontName       = "i16";
  Inconsolata16.mFontFile       = "lev2://textures/Inconsolata16";
  Inconsolata16.miTexWidth      = 512;
  Inconsolata16.miTexHeight     = 512;
  Inconsolata16.miCellWidth     = (512 / 16);
  Inconsolata16.miCellHeight    = (512 / 16);
  Inconsolata16.miCharWidth     = 16;
  Inconsolata16.miCharHeight    = 16;
  Inconsolata16.miCharOffsetX   = 11;
  Inconsolata16.miCharOffsetY   = 11;
  Inconsolata16.miYShift        = 1;
  Inconsolata16.miAdvanceWidth  = 8;
  Inconsolata16.miAdvanceHeight = 12;

  FontDesc Inconsolata24;
  Inconsolata24.mFontName       = "i24";
  Inconsolata24.mFontFile       = "lev2://textures/Inconsolata24";
  Inconsolata24.miTexWidth      = 512;
  Inconsolata24.miTexHeight     = 512;
  Inconsolata24.miCellWidth     = (512 / 16);
  Inconsolata24.miCellHeight    = (512 / 16);
  Inconsolata24.miCharWidth     = 24;
  Inconsolata24.miCharHeight    = 24;
  Inconsolata24.miCharOffsetX   = 9;
  Inconsolata24.miCharOffsetY   = 0;
  Inconsolata24.miYShift        = -5;
  Inconsolata24.miAdvanceWidth  = 11;
  Inconsolata24.miAdvanceHeight = 20;

  FontDesc Inconsolata32;
  Inconsolata32.mFontName       = "i32";
  Inconsolata32.mFontFile       = "lev2://textures/Inconsolata32";
  Inconsolata32.miTexWidth      = 800;
  Inconsolata32.miTexHeight     = 800;
  Inconsolata32.miCellWidth     = (800 / 16);
  Inconsolata32.miCellHeight    = (800 / 16);
  Inconsolata32.miCharWidth     = 32;
  Inconsolata32.miCharHeight    = 32;
  Inconsolata32.miCharOffsetX   = 0;
  Inconsolata32.miCharOffsetY   = 0;
  Inconsolata32.miYShift        = -4;
  Inconsolata32.miAdvanceWidth  = 16;
  Inconsolata32.miAdvanceHeight = 24;

  FontDesc Inconsolata48;
  Inconsolata48.mFontName       = "i48";
  Inconsolata48.mFontFile       = "lev2://textures/Inconsolata48";
  Inconsolata48.miTexWidth      = 800;
  Inconsolata48.miTexHeight     = 800;
  Inconsolata48.miCellWidth     = (800 / 16);
  Inconsolata48.miCellHeight    = (800 / 16);
  Inconsolata48.miCharWidth     = 48;
  Inconsolata48.miCharHeight    = 48;
  Inconsolata48.miCharOffsetX   = 0;
  Inconsolata48.miCharOffsetY   = 0;
  Inconsolata48.miYShift        = -7;
  Inconsolata48.miAdvanceWidth  = 24;
  Inconsolata48.miAdvanceHeight = 40;

  FontDesc Transponder24;
  Transponder24.mFontName       = "d24";
  Transponder24.mFontFile       = "lev2://textures/transponder24";
  Transponder24.miTexWidth      = 512;
  Transponder24.miTexHeight     = 512;
  Transponder24.miCellWidth     = (512 / 16);
  Transponder24.miCellHeight    = (512 / 16);
  Transponder24.miCharWidth     = 24;
  Transponder24.miCharHeight    = 24;
  Transponder24.miCharOffsetX   = 9;
  Transponder24.miCharOffsetY   = 7;
  Transponder24.miAdvanceWidth  = 16;
  Transponder24.miAdvanceHeight = 24;

  if (_doGpuInit) {
    pTARG->makeCurrentContext();
    pTARG->debugPushGroup("FontMan::InitFonts");

    _addFont(pTARG, Inconsolata12);
    _addFont(pTARG, Inconsolata13);
    _addFont(pTARG, Inconsolata14);
    _addFont(pTARG, Inconsolata16);
    _addFont(pTARG, Inconsolata24);
    _addFont(pTARG, Inconsolata32);
    _addFont(pTARG, Inconsolata48);
    _addFont(pTARG, Transponder24);

    _pushFont("i14");
    pTARG->debugPopGroup();
    _doGpuInit = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_addFont(Context* pTARG, const FontDesc& fdesc) {
  Font* pFontAlreadyLoaded = OldStlSchoolFindValFromKey(mFontMap, fdesc.mFontName, (Font*)0);

  if (0 == pFontAlreadyLoaded) {
    Font* pNewFont = new Font(fdesc.mFontName, fdesc.mFontFile);

    pNewFont->LoadFromDisk(pTARG, fdesc);

    mFontVect.push_back(pNewFont);
    OldStlSchoolMapInsert(mFontMap, fdesc.mFontName, pNewFont);
    mpCurrentFont = pNewFont;
  }
}

///////////////////////////////////////////////////////////////////////////////
void FontMan::_drawText(Context* pTARG, int iX, int iY, const fixedstring_t& text) {
  ///////////////////////////////////
  const char* textbuffer = text.c_str();
  Font* pFont            = mpCurrentFont;
  size_t iLen            = strlen(textbuffer);
  const FontDesc& fdesc  = pFont->GetFontDesc();

  int iSX     = fdesc.miAdvanceWidth;
  int iSY     = fdesc.miAdvanceHeight;
  int ishifty = fdesc.miYShift;

  auto VPRect = pTARG->FBI()->viewport();

  ///////////////////////////////////
  int iRow = 0;
  int iCol = 0;

  for (size_t i = 0; i < iLen; i++) {
    char ch = textbuffer[i];

    switch (ch) {
      case 0x0a: // linefeed
      {
        iRow++;
        iCol = 0;
        break;
      }
      case 0x20: // space
      {
        iCol++;
        break;
      }
      default: {
        int iCharRow = -1;
        int iCharCol = -1;

        const CharDesc& desc = mCharDescriptions[int(ch)];

        if (desc.ch != 0) {
          iCharRow = desc.miRow;
          iCharCol = desc.miCol;
        }

        if ((iCharRow >= 0) && (iCharCol >= 0)) {
          int iX0 = iX + (iCol * iSX);
          int iY0 = iY + (iRow * iSY) + ishifty;
          // pFont->QueChar( pTARG, GetRef().mTextWriter, iX0, iY0, iCharCol, iCharRow, pTARG->RefModColor().GetABGRU32() );
          pFont->QueChar(pTARG, mTextWriter, iX0, iY0, iCharCol, iCharRow, pTARG->RefModColor().GetBGRAU32());
          iCol++;
        }

        break;
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::DrawText(Context* pTARG, int iX, int iY, const char* pFmt, ...) {
  fixedstring_t fxs;
  va_list argp;
  va_start(argp, pFmt);
  vsnprintf(fxs.mutable_c_str(), KFIXEDSTRINGLEN, pFmt, argp);
  va_end(argp);
  instance()->_drawText(pTARG, iX, iY, fxs);
}

///////////////////////////////////////////////////////////////////////////////

Font::Font(const std::string& fontname, const std::string& filename)
    : msFileName(filename)
    , msFontName(fontname) {
}

///////////////////////////////////////////////////////////////////////////////

void Font::QueChar(Context* pTARG, VtxWriter<SVtxV12C4T16>& vw, int ix, int iy, int iu, int iv, U32 ucolor) {
  const float kftexsiz   = float(mFontDesc.miTexWidth);
  const float kfU0offset = +0.0f;
  const float kfV0offset = 0.0f;
  const float kfU1offset = -0.0f;
  const float kfV1offset = 0.0f;
  ///////////////////////////////////////////////
  const float kfcharW = float(mFontDesc.miCharWidth);
  const float kfcharH = float(mFontDesc.miCharHeight);
  const float kfcellW = float(mFontDesc.miCellWidth);
  const float kfcellH = float(mFontDesc.miCellHeight);
  ///////////////////////////////////////////////
  // calc vertex pos
  ///////////////////////////////////////////////
  float fix1 = float(ix);
  float fiy1 = float(iy);
  ///////////////////////////////////////////////
  // calc UVs
  ///////////////////////////////////////////////
  float fiu        = float(iu);
  float fiv        = float(iv);
  float fcellbaseU = kfcellW * fiu;
  float fcellbaseV = kfcellH * fiv;
  const float fix2 = fix1 + kfcharW;
  const float fiy2 = fiy1 + kfcharH;
  float fu1        = fcellbaseU + float(mFontDesc.miCharOffsetX);
  float fv1        = fcellbaseV + float(mFontDesc.miCharOffsetY);
  float fu2        = fu1 + kfcharW;
  float fv2        = fv1 + kfcharH;
  //
  float kitexs = 1.0f / kftexsiz;
  fu1 *= kitexs;
  fu2 *= kitexs;
  fv1 *= kitexs;
  fv2 *= kitexs;
  ///////////////////////////////////////////////
  /*printf(
      "queuechar font<%s> ix<%d> iy<%d> iu<%d> iv<%d> fix1<%g> fiy1<%g> fix2<%g> fiy2<%g> fu1<%g> fu2<%g>\n",
      msFontName.c_str(),
      ix,
      iy,
      iu,
      iv,
      fix1,
      fiy1,
      fix2,
      fiy2,
      fu1,
      fu2);*/
  ///////////////////////////////////////////////
  vw.AddVertex(TEXT_VTXFMT(fix1, fiy1, 0.0f, fu1, fv1, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix2, fiy1, 0.0f, fu2, fv1, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix2, fiy2, 0.0f, fu2, fv2, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix1, fiy1, 0.0f, fu1, fv1, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix2, fiy2, 0.0f, fu2, fv2, ucolor));
  vw.AddVertex(TEXT_VTXFMT(fix1, fiy2, 0.0f, fu1, fv2, ucolor));
}

///////////////////////////////////////////////////////////////////////////////

void Font::LoadFromDisk(Context* pTARG, const FontDesc& fdesc) {
  mpMaterial = new GfxMaterialUIText;
  mpMaterial->Init(pTARG);
  AssetPath apath(msFileName.c_str());
  auto tex_asset = asset::AssetManager<ork::lev2::TextureAsset>::Load(apath.c_str());
  auto ptex      = tex_asset->GetTexture();
  mpMaterial->SetTexture(ETEXDEST_DIFFUSE, ptex);
  ptex->TexSamplingMode().PresetPointAndClamp();
  pTARG->TXI()->ApplySamplingMode(ptex);
  mFontDesc = fdesc;
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
