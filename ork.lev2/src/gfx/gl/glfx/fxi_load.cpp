////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

rootcontainer_ptr_t LoadFxFromFile(const AssetPath& pth) {
  auto scanner = std::make_shared<Scanner>(block_regex);
  ///////////////////////////////////
  File fx_file(pth, EFM_READ);
  OrkAssert(fx_file.IsOpen());
  EFileErrCode eFileErr = fx_file.GetLength(scanner->ifilelen);
  scanner->resize(scanner->ifilelen + 1);
  eFileErr = fx_file.Read(scanner->_fxbuffer.data(), 
                          scanner->ifilelen);
  scanner->_fxbuffer[scanner->ifilelen] = 0;
  ///////////////////////////////////
  parser::performScan(scanner);
  auto program = std::make_shared<parser::Program>(pth.c_str());
  auto parser = std::make_shared<parser::GlSlFxParser>(pth.c_str(), 
                                               program,
                                               scanner);
  ///////////////////////////////////
  auto topnode = parser->_topNode;
  auto pcont = std::make_shared<RootContainer>(pth.c_str());
  shaderbuilder::BackEnd backend(parser, pcont);
  bool ok = backend.generate();
  assert(ok);
  ///////////////////////////////////
  return pcont;
}

///////////////////////////////////////////////////////////////////////////////

rootcontainer_ptr_t LoadFxFromText(const std::string& name, const std::string& shadertext) {
  auto scanner = std::make_shared<Scanner>(block_regex);
  ///////////////////////////////////
  scanner->ifilelen = shadertext.length();
  scanner->resize(scanner->ifilelen + 1);
  memcpy(scanner->_fxbuffer.data(), 
         shadertext.c_str(), 
         scanner->ifilelen);
  scanner->_fxbuffer[scanner->ifilelen] = 0;
  ///////////////////////////////////
  parser::performScan(scanner);
  auto program = std::make_shared<parser::Program>(name);
  auto parser = std::make_shared<parser::GlSlFxParser>(name,
                                               program,
                                               scanner);
  ///////////////////////////////////
  auto topnode = parser->_topNode;
  auto pcont = std::make_shared<RootContainer>(name);
  shaderbuilder::BackEnd backend(parser, pcont);
  bool ok = backend.generate();
  assert(ok);
  ///////////////////////////////////
  return pcont;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
