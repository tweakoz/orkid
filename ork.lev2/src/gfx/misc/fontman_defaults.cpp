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

void FontMan::installDefaults() {

  auto Inconsolata12 = std::make_shared<FontDesc>();
  Inconsolata12->mFontName       = "i12";
  Inconsolata12->mFontFile       = "lev2://textures/Inconsolata12";
  Inconsolata12->miTexWidth      = 512;
  Inconsolata12->miTexHeight     = 512;
  Inconsolata12->miCellWidth     = (512 / 16);
  Inconsolata12->miCellHeight    = (512 / 16);
  Inconsolata12->miCharWidth     = 12;
  Inconsolata12->miCharHeight    = 12;
  Inconsolata12->miCharOffsetX   = 12;
  Inconsolata12->miCharOffsetY   = 12;
  Inconsolata12->miAdvanceWidth  = 6;
  Inconsolata12->miAdvanceHeight = 12;

  /////////////////////////////////////////////////////////////

  auto Inconsolata13 = std::make_shared<FontDesc>();
  Inconsolata13->mFontName       = "i13";
  Inconsolata13->mFontFile       = "lev2://textures/Inconsolata13";
  Inconsolata13->miTexWidth      = 512;
  Inconsolata13->miTexHeight     = 512;
  Inconsolata13->miCellWidth     = (512 / 16);
  Inconsolata13->miCellHeight    = (512 / 16);
  Inconsolata13->miCharWidth     = 13;
  Inconsolata13->miCharHeight    = 13;
  Inconsolata13->miCharOffsetX   = 11;
  Inconsolata13->miCharOffsetY   = 11;
  Inconsolata13->miAdvanceWidth  = 7;
  Inconsolata13->miAdvanceHeight = 13;
  Inconsolata13->miYShift        = -1;

  /////////////////////////////////////////////////////////////

  auto Inconsolata14 = std::make_shared<FontDesc>();
  Inconsolata14->mFontName       = "i14";
  Inconsolata14->mFontFile       = "lev2://textures/Inconsolata14";
  Inconsolata14->miTexWidth      = 512;
  Inconsolata14->miTexHeight     = 512;
  Inconsolata14->miCellWidth     = (512 / 16);
  Inconsolata14->miCellHeight    = (512 / 16);
  Inconsolata14->miCharWidth     = 14;
  Inconsolata14->miCharHeight    = 14;
  Inconsolata14->miCharOffsetX   = 12;
  Inconsolata14->miCharOffsetY   = 8;
  Inconsolata14->miYShift        = -1;
  Inconsolata14->miAdvanceWidth  = 7;
  Inconsolata14->miAdvanceHeight = 12;
  //
  Inconsolata14->_3d_char_width    = 7;
  Inconsolata14->_3d_char_height   = 8;
  Inconsolata14->_3d_char_u_offset = 12;
  Inconsolata14->_3d_char_v_offset = 9;
  Inconsolata14->_3d_char_u_width  = 8;
  Inconsolata14->_3d_char_v_height = 11;

  /////////////////////////////////////////////////////////////

  auto Inconsolata16 = std::make_shared<FontDesc>();
  Inconsolata16->mFontName       = "i16";
  Inconsolata16->mFontFile       = "lev2://textures/Inconsolata16";
  Inconsolata16->miTexWidth      = 512;
  Inconsolata16->miTexHeight     = 512;
  Inconsolata16->miCellWidth     = (512 / 16);
  Inconsolata16->miCellHeight    = (512 / 16);
  Inconsolata16->miCharWidth     = 16;
  Inconsolata16->miCharHeight    = 16;
  Inconsolata16->miCharOffsetX   = 11;
  Inconsolata16->miCharOffsetY   = 11;
  Inconsolata16->miYShift        = 2;
  Inconsolata16->miAdvanceWidth  = 8;
  Inconsolata16->miAdvanceHeight = 12;

  /////////////////////////////////////////////////////////////

  auto Inconsolata17 = std::make_shared<FontDesc>();
  Inconsolata17->mFontName       = "i17";
  Inconsolata17->mFontFile       = "lev2://textures/Inconsolata17";
  Inconsolata17->miTexWidth      = 608;
  Inconsolata17->miTexHeight     = 608;
  Inconsolata17->miCellWidth     = 38;
  Inconsolata17->miCellHeight    = 38;
  Inconsolata17->miCharWidth     = 17;
  Inconsolata17->miCharHeight    = 17;
  Inconsolata17->miCharOffsetX   = 15;
  Inconsolata17->miCharOffsetY   = 14;
  Inconsolata17->miYShift        = 0;
  Inconsolata17->miAdvanceWidth  = 8;
  Inconsolata17->miAdvanceHeight = 13;

  /////////////////////////////////////////////////////////////

  auto Inconsolata18 = std::make_shared<FontDesc>();
  Inconsolata18->mFontName       = "i18";
  Inconsolata18->mFontFile       = "lev2://textures/Inconsolata18";
  Inconsolata18->miTexWidth      = 624;
  Inconsolata18->miTexHeight     = 624;
  Inconsolata18->miCellWidth     = 39;
  Inconsolata18->miCellHeight    = 39;
  Inconsolata18->miCharWidth     = 18;
  Inconsolata18->miCharHeight    = 18;
  Inconsolata18->miCharOffsetX   = 16;
  Inconsolata18->miCharOffsetY   = 14;
  Inconsolata18->miYShift        = 3;
  Inconsolata18->miAdvanceWidth  = 9;
  Inconsolata18->miAdvanceHeight = 15;

  /////////////////////////////////////////////////////////////

  auto Inconsolata20 = std::make_shared<FontDesc>();
  Inconsolata20->mFontName       = "i20";
  Inconsolata20->mFontFile       = "lev2://textures/Inconsolata20";
  Inconsolata20->miTexWidth      = 672;
  Inconsolata20->miTexHeight     = 672;
  Inconsolata20->miCellWidth     = 42;
  Inconsolata20->miCellHeight    = 42;
  Inconsolata20->miCharWidth     = 20;
  Inconsolata20->miCharHeight    = 20;
  Inconsolata20->miCharOffsetX   = 11;
  Inconsolata20->miCharOffsetY   = 11;
  Inconsolata20->miYShift        = -1;
  Inconsolata20->miAdvanceWidth  = 10;
  Inconsolata20->miAdvanceHeight = 17;

  /////////////////////////////////////////////////////////////

  auto Inconsolata22 = std::make_shared<FontDesc>();
  Inconsolata22->mFontName       = "i22";
  Inconsolata22->mFontFile       = "lev2://textures/Inconsolata22";
  Inconsolata22->miTexWidth      = 720;
  Inconsolata22->miTexHeight     = 720;
  Inconsolata22->miCellWidth     = 45;
  Inconsolata22->miCellHeight    = 45;
  Inconsolata22->miCharWidth     = 22;
  Inconsolata22->miCharHeight    = 22;
  Inconsolata22->miCharOffsetX   = 11;
  Inconsolata22->miCharOffsetY   = 11;
  Inconsolata22->miYShift        = -1;
  Inconsolata22->miAdvanceWidth  = 11;
  Inconsolata22->miAdvanceHeight = 19;

  /////////////////////////////////////////////////////////////

  auto Inconsolata24 = std::make_shared<FontDesc>();
  Inconsolata24->mFontName       = "i24";
  Inconsolata24->mFontFile       = "lev2://textures/Inconsolata24";
  Inconsolata24->miTexWidth      = 512;
  Inconsolata24->miTexHeight     = 512;
  Inconsolata24->miCellWidth     = (512 / 16);
  Inconsolata24->miCellHeight    = (512 / 16);
  Inconsolata24->miCharWidth     = 24;
  Inconsolata24->miCharHeight    = 24;
  Inconsolata24->miCharOffsetX   = 9;
  Inconsolata24->miCharOffsetY   = 3;
  Inconsolata24->miYShift        = 0;
  Inconsolata24->miAdvanceWidth  = 11;
  Inconsolata24->miAdvanceHeight = 21;

  /////////////////////////////////////////////////////////////

  auto Inconsolata26 = std::make_shared<FontDesc>();
  Inconsolata26->mFontName       = "i26";
  Inconsolata26->mFontFile       = "lev2://textures/Inconsolata26";
  Inconsolata26->miTexWidth      = 816;
  Inconsolata26->miTexHeight     = 816;
  Inconsolata26->miCellWidth     = 51;
  Inconsolata26->miCellHeight    = 51;
  Inconsolata26->miCharWidth     = 26;
  Inconsolata26->miCharHeight    = 26;
  Inconsolata26->miCharOffsetX   = 12;
  Inconsolata26->miCharOffsetY   = 12;
  Inconsolata26->miYShift        = 0;
  Inconsolata26->miAdvanceWidth  = 13;
  Inconsolata26->miAdvanceHeight = 26;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata28 = std::make_shared<FontDesc>();
  Inconsolata28->mFontName       = "i28";
  Inconsolata28->mFontFile       = "lev2://textures/Inconsolata28";
  Inconsolata28->miTexWidth      = 864;
  Inconsolata28->miTexHeight     = 864;
  Inconsolata28->miCellWidth     = 54;
  Inconsolata28->miCellHeight    = 54;
  Inconsolata28->miCharWidth     = 28;
  Inconsolata28->miCharHeight    = 28;
  Inconsolata28->miCharOffsetX   = 13;
  Inconsolata28->miCharOffsetY   = 13;
  Inconsolata28->miYShift        = 0;
  Inconsolata28->miAdvanceWidth  = 14;
  Inconsolata28->miAdvanceHeight = 28;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata30 = std::make_shared<FontDesc>();
  Inconsolata30->mFontName       = "i30";
  Inconsolata30->mFontFile       = "lev2://textures/Inconsolata30";
  Inconsolata30->miTexWidth      = 912;
  Inconsolata30->miTexHeight     = 912;
  Inconsolata30->miCellWidth     = 57;
  Inconsolata30->miCellHeight    = 57;
  Inconsolata30->miCharWidth     = 30;
  Inconsolata30->miCharHeight    = 30;
  Inconsolata30->miCharOffsetX   = 13;
  Inconsolata30->miCharOffsetY   = 13;
  Inconsolata30->miYShift        = 0;
  Inconsolata30->miAdvanceWidth  = 15;
  Inconsolata30->miAdvanceHeight = 30;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata32 = std::make_shared<FontDesc>();
  Inconsolata32->mFontName       = "i32";
  Inconsolata32->mFontFile       = "lev2://textures/Inconsolata32";
  Inconsolata32->miTexWidth      = 800;
  Inconsolata32->miTexHeight     = 800;
  Inconsolata32->miCellWidth     = (800 / 16);
  Inconsolata32->miCellHeight    = (800 / 16);
  Inconsolata32->miCharWidth     = 32;
  Inconsolata32->miCharHeight    = 32;
  Inconsolata32->miCharOffsetX   = 0;
  Inconsolata32->miCharOffsetY   = 0;
  Inconsolata32->miYShift        = -1;
  Inconsolata32->miAdvanceWidth  = 16;
  Inconsolata32->miAdvanceHeight = 23;

  /////////////////////////////////////////////////////////////

  auto Inconsolata34 = std::make_shared<FontDesc>();
  Inconsolata34->mFontName       = "i34";
  Inconsolata34->mFontFile       = "lev2://textures/Inconsolata34";
  Inconsolata34->miTexWidth      = 1008;
  Inconsolata34->miTexHeight     = 1008;
  Inconsolata34->miCellWidth     = 63;
  Inconsolata34->miCellHeight    = 63;
  Inconsolata34->miCharWidth     = 34;
  Inconsolata34->miCharHeight    = 34;
  Inconsolata34->miCharOffsetX   = 14;
  Inconsolata34->miCharOffsetY   = 14;
  Inconsolata34->miYShift        = 0;
  Inconsolata34->miAdvanceWidth  = 17;
  Inconsolata34->miAdvanceHeight = 34;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata36 = std::make_shared<FontDesc>();
  Inconsolata36->mFontName       = "i36";
  Inconsolata36->mFontFile       = "lev2://textures/Inconsolata36";
  Inconsolata36->miTexWidth      = 1056;
  Inconsolata36->miTexHeight     = 1056;
  Inconsolata36->miCellWidth     = 66;
  Inconsolata36->miCellHeight    = 66;
  Inconsolata36->miCharWidth     = 36;
  Inconsolata36->miCharHeight    = 36;
  Inconsolata36->miCharOffsetX   = 15;
  Inconsolata36->miCharOffsetY   = 15;
  Inconsolata36->miYShift        = 0;
  Inconsolata36->miAdvanceWidth  = 18;
  Inconsolata36->miAdvanceHeight = 36;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata38 = std::make_shared<FontDesc>();
  Inconsolata38->mFontName       = "i38";
  Inconsolata38->mFontFile       = "lev2://textures/Inconsolata38";
  Inconsolata38->miTexWidth      = 1104;
  Inconsolata38->miTexHeight     = 1104;
  Inconsolata38->miCellWidth     = 69;
  Inconsolata38->miCellHeight    = 69;
  Inconsolata38->miCharWidth     = 38;
  Inconsolata38->miCharHeight    = 38;
  Inconsolata38->miCharOffsetX   = 15;
  Inconsolata38->miCharOffsetY   = 15;
  Inconsolata38->miYShift        = 0;
  Inconsolata38->miAdvanceWidth  = 19;
  Inconsolata38->miAdvanceHeight = 38;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata40 = std::make_shared<FontDesc>();
  Inconsolata40->mFontName       = "i40";
  Inconsolata40->mFontFile       = "lev2://textures/Inconsolata40";
  Inconsolata40->miTexWidth      = 1152;
  Inconsolata40->miTexHeight     = 1152;
  Inconsolata40->miCellWidth     = 72;
  Inconsolata40->miCellHeight    = 72;
  Inconsolata40->miCharWidth     = 40;
  Inconsolata40->miCharHeight    = 40;
  Inconsolata40->miCharOffsetX   = 16;
  Inconsolata40->miCharOffsetY   = 16;
  Inconsolata40->miYShift        = 0;
  Inconsolata40->miAdvanceWidth  = 20;
  Inconsolata40->miAdvanceHeight = 40;
  
  /////////////////////////////////////////////////////////////

  auto Inconsolata48 = std::make_shared<FontDesc>();
  Inconsolata48->mFontName       = "i48";
  Inconsolata48->mFontFile       = "lev2://textures/Inconsolata48";
  Inconsolata48->miTexWidth      = 800;
  Inconsolata48->miTexHeight     = 800;
  Inconsolata48->miCellWidth     = (800 / 16);
  Inconsolata48->miCellHeight    = (800 / 16);
  Inconsolata48->miCharWidth     = 48;
  Inconsolata48->miCharHeight    = 48;
  Inconsolata48->miCharOffsetX   = 0;
  Inconsolata48->miCharOffsetY   = 0;
  Inconsolata48->miYShift        = -7;
  Inconsolata48->miAdvanceWidth  = 24;
  Inconsolata48->miAdvanceHeight = 40;
  //
  Inconsolata48->_3d_char_width    = 48;
  Inconsolata48->_3d_char_height   = 48;
  Inconsolata48->_3d_char_u_offset = 1;
  Inconsolata48->_3d_char_v_offset = 6;
  Inconsolata48->_3d_char_u_width  = 48;
  Inconsolata48->_3d_char_v_height = 48;

  /////////////////////////////////////////////////////////////

  auto Transponder24 = std::make_shared<FontDesc>();
  Transponder24->mFontName       = "d24";
  Transponder24->mFontFile       = "lev2://textures/transponder24";
  Transponder24->miTexWidth      = 512;
  Transponder24->miTexHeight     = 512;
  Transponder24->miCellWidth     = (512 / 16);
  Transponder24->miCellHeight    = (512 / 16);
  Transponder24->miCharWidth     = 24;
  Transponder24->miCharHeight    = 24;
  Transponder24->miCharOffsetX   = 9;
  Transponder24->miCharOffsetY   = 7;
  Transponder24->miAdvanceWidth  = 16;
  Transponder24->miAdvanceHeight = 24;

  _addFont(Inconsolata12);
  _addFont(Inconsolata13);
  _addFont(Inconsolata14);
  _addFont(Inconsolata16);
  _addFont(Inconsolata17);
  _addFont(Inconsolata18);
  _addFont(Inconsolata20);
  _addFont(Inconsolata22);
  _addFont(Inconsolata24);
  _addFont(Inconsolata26);
  _addFont(Inconsolata28);
  _addFont(Inconsolata30);
  _addFont(Inconsolata32);
  _addFont(Inconsolata48);
  _addFont(Transponder24);

}
} //namespace ork { namespace lev2 {
