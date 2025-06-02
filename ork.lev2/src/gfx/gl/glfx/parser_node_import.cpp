////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ImportNode::pregen(shaderbuilder::BackEnd& backend) const {
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ImportNode::load(const file::Path& resolvedPath) {
  auto parent_parser = _parent_topnode->_parser;
  auto program       = parent_parser->_program;
  //printf( "  IMPORT %s\n",  resolvedPath.c_str());
  auto importscanner = std::make_shared<Scanner>(block_regex);
  ///////////////////////////////////
  File fx_file(resolvedPath.c_str(), EFM_READ);
  ///////////////////////////////////
  OrkAssert(fx_file.IsOpen());
  EFileErrCode eFileErr = fx_file.GetLength(importscanner->ifilelen);
  importscanner->resize(importscanner->ifilelen + 1);
  eFileErr                                          = fx_file.Read(importscanner->_fxbuffer.data(), importscanner->ifilelen);
  importscanner->_fxbuffer[importscanner->ifilelen] = 0;
  performScan(importscanner);
  _parser = std::make_shared<GlSlFxParser>(resolvedPath.c_str(), program, importscanner);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////
