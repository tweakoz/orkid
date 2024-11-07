////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/kernel/string/deco.inl>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/debug.h>

#include "../gl.h"
#include <string>
#include <vector>
#include <cctype>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

#pragma once


enum TokenType {
  Keyword,
  Identifier,
  Number,
  String,
  Operator,
  Punctuation,
  Whitespace,
  Comment,
  Preprocessor,
  BuiltInMethod,
  BuiltInType,
  Unknown
};

struct Token {
  TokenType type;
  std::string text;
};

struct Keywords{
  Keywords();
  std::unordered_set<std::string> _keywords;
  std::unordered_set<std::string> _builtinmethods;
  std::unordered_set<std::string> _builtintypes;
};

struct ShaderAttrib {
  std::string _name;
  GLint _size  = 0;
  GLenum _type = GL_NONE;
  GLint _location = -1;
};
using shader_attrib_ptr_t = std::shared_ptr<ShaderAttrib>;

std::vector<Token> LexLine(const std::string& line);
ftxui::Decorator GetTokenDecorator(TokenType type);
ftxui::Element SyntaxHighlightLine(const std::string& line);

struct irgb {
  int r;
  int g;
  int b;
};
static constexpr irgb RED = irgb{255, 0, 0};
static constexpr irgb YEL = irgb{255, 255, 0};
static constexpr irgb BLK = irgb{0, 0, 0};
static constexpr irgb BLU1 = irgb{0, 0, 32};
static constexpr irgb BLU2 = irgb{0, 0, 64};
static constexpr irgb BLU3 = irgb{0, 0, 128};
static constexpr irgb BLU4 = irgb{0, 0, 192};
static constexpr irgb WHI = irgb{255, 255, 255};
static constexpr irgb GR1 = irgb{32, 32, 32};
static constexpr irgb GR2 = irgb{64, 64, 64};
static constexpr irgb GR3 = irgb{128, 128, 128};
static constexpr irgb GRN = irgb{0, 255, 0};
static constexpr irgb CYN = irgb{0, 255, 255};
static constexpr irgb MAG = irgb{255, 0, 255};
static constexpr irgb ORA = irgb{255, 192, 32};

void _colortext(ftxui::node_vect_t& NODES, irgb foreground, irgb background, const char* formatstring, ...);
void _colortext_wrap(ftxui::node_vect_t& NODES, irgb foreground, irgb background, const char* formatstring, ...);
ftxui::component_ptr_t code_viewer(std::vector<std::string> lines);
ftxui::component_ptr_t Wrap(std::string name, ftxui::component_ptr_t comp_in);

struct _FtxGlDebugger {

  _FtxGlDebugger(const ContextGL* gl);
  void run_loop();

  void _validateRaster();
  void _validateCurrentShaderProgram();
  void _validateCurrentFramebuffer();
  void _validateCurrentGeomBuffers();
  void _validateTextureBindingState();
  void _validateTextures();

  const ContextGL* _glctx;
  std::shared_ptr<ftxui::ScreenInteractive> _fxtui_screen;
  ftxui::component_ptr_t _comp_top;
  ftxui::node_ptr_t _node_backtrace;
  ftxui::node_ptr_t _node_framebuffer;
  ftxui::node_ptr_t _node_shader;
  ftxui::node_ptr_t _node_raster;
  ftxui::node_ptr_t _node_geometry;
  ftxui::node_ptr_t _node_texturebindingstate;
  ftxui::node_ptr_t _node_textures;
  using shader_text_t = std::vector<std::string>;

  std::map<int,shader_attrib_ptr_t> _shader_attribs;
  std::map<std::string,shader_text_t> _shader_texts;
};
///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
