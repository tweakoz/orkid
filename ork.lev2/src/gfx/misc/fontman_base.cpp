////////////////////////////////////////////////////////////////
// Copyright 2007, Michael T. Mayers, all rights reserved.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/targetinterfaces.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

fontman_ptr_t FontMan::instance() {
  struct PublicFontMan : public FontMan {
    PublicFontMan() {
      //printf("PublicFontMan instantiated<%p>...\n", this);
    }
  };
  static std::shared_ptr<PublicFontMan> _instance = std::make_shared<PublicFontMan>();
  // printf("FontMan::instance<%p>\n", _instance.get());
  return _instance;
}

///////////////////////////////////////////////////////////////////////////////

FontMan& FontMan::GetRef() {
  return *instance();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FontMan::FontMan() {

  for (int ich = 0; ich <= 255; ich++) {
    CharDesc& desc = mCharDescriptions[ich];
    desc.miRow     = ich >> 4;
    desc.miCol     = ich % 16;
    desc.ch        = (char)ich;
  }

  auto tbstate           = std::make_shared<TextBlockState>();
  _defaultTextBlockState = tbstate;
  _textBlockStateStack.push(tbstate);
  _currentTextBlockState = tbstate;

  installDefaults();
}

///////////////////////////////////////////////////////////////////////////////

FontMan::~FontMan() {
  _fontmap.atomicOp([this](font_byname_map_t& unlocked) {
    unlocked.clear();
  });
}

///////////////////////////////////////////////////////////////////////////////

int FontMan::stringWidth(int numchars) {
  auto font = currentFont();
  return font->_fontdesc->stringWidth(numchars);
}
int FontMan::stringHeight(int numlines) {
  auto font = currentFont();
  return font->_fontdesc->stringHeight(numlines);
}

///////////////////////////////////////////////////////////////////////////////
// Font Management
///////////////////////////////////////////////////////////////////////////////

void FontMan::_addFont(fontdesc_ptr_t fdesc) {
  _fontmap.atomicOp([this, &fdesc](font_byname_map_t& unlocked) {
    auto it = unlocked.find(fdesc->mFontName);
    if (it == unlocked.end()) {
      auto new_font = std::make_shared<Font>(fdesc->mFontName, fdesc->mFontFile);
      _fontvect.push_back(new_font);
      unlocked[fdesc->mFontName] = new_font;
      new_font->_fontdesc = fdesc;
    }
  });
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::currentFont() {
  auto top_state = instance()->_currentTextBlockState;
  return top_state->_font;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::fontForId(const std::string& name) {
  auto fontman = instance();
  font_ptr_t rval = nullptr;
  fontman->_fontmap.atomicOp([&](font_byname_map_t& unlocked) {
    auto it = unlocked.find(name);
    if (it != unlocked.end()) {
          rval = it->second;
    } else {
      orkprintf("FontMan::fontForId<%s> not found\n", name.c_str());
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::setCurrentFont(const std::string& name) {
  auto fontman      = instance();
  font_ptr_t font = fontForId(name);
  auto top_state   = fontman->_currentTextBlockState;
  top_state->_font = font;
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::PushFont(font_ptr_t font) {
  OrkAssert(font);
  auto top_state = instance()->_currentTextBlockState;
  top_state->_fontstack.push(top_state->_font);
  top_state->_font = font;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::PushFont(const std::string& name) {
  return instance()->_pushFont(name);
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::PopFont() {
  return instance()->_popFont();
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_bindFont(font_ptr_t pFont) {
  OrkAssert(pFont);
  _currentTextBlockState->_font = pFont;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::_pushFont(const std::string& name) {
  auto the_font = fontForId(name);
  if (the_font == nullptr) {
    printf("FontMan::_pushFont<%s> not found\n", name.c_str());
  }
  OrkAssert(the_font);
  auto top_state    = _currentTextBlockState;
  auto current_font = top_state->_font;
  top_state->_fontstack.push(current_font);
  top_state->_font = the_font;
  return the_font;
}

///////////////////////////////////////////////////////////////////////////////

font_ptr_t FontMan::_popFont() {
  auto top_state = _currentTextBlockState;
  top_state->_fontstack.pop();
  auto next_font = top_state->_fontstack.top();
  OrkAssert(next_font);
  top_state->_font = next_font;
  return next_font;
}

///////////////////////////////////////////////////////////////////////////////

textblockstate_ptr_t FontMan::_topstate() {
  return _currentTextBlockState;
}

///////////////////////////////////////////////////////////////////////////////

// Inconsolata font from http://www.levien.com/type/myfonts/inconsolata.html
// font textures built with F2IBuilder http://sourceforge.net/projects/f2ibuilder/
//  set texture size to 512
//  use metrics off
//  use a monospace font
//  show grid should be fine so long as the font stays away from the cell boundaries
//  a 'cell' is one of the 256(16x16) tiles in the texture,
//   if the texture size is 512x512, then a single cell will be 32x32

///////////////////////////////////////////////////////////////////////////////

void FontMan::gpuInit(Context* pTARG) {
  instance()->_gpuInit(pTARG);
}

///////////////////////////////////////////////////////////////////////////////

void FontMan::_gpuInit(Context* pTARG) {

  /////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////////

  if (_doGpuInit) {
    pTARG->makeCurrentContext();
    //pTARG->debugPushGroup("FontMan::InitFonts");

    for( auto font : _fontvect ){
      font->load(pTARG, font->_fontdesc);
      _bindFont(font);
    }

    _defaultTextBlockState->_font = _pushFont("i14");

    //pTARG->debugPopGroup();
    _doGpuInit = false;
  }
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
