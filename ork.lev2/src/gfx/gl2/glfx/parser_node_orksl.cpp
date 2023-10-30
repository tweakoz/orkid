////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

std::string FnParseContext::tokenValue(size_t offset) const {
  return _view->token(_startIndex + offset)->text;
}

FnParseContext::FnParseContext(GlSlFxParser* parser, const ScannerView* v)
    : _parser(parser)
    , _view(v) {
}
FnParseContext::FnParseContext(const FnParseContext& oth)
    : _parser(oth._parser)
    , _startIndex(oth._startIndex)
    , _view(oth._view) {
}
FnParseContext& FnParseContext::operator=(const FnParseContext& oth) {
  _parser     = oth._parser;
  _startIndex = oth._startIndex;
  _view       = oth._view;
  return *this;
}
FnParseContext FnParseContext::advance(size_t count) const {
  FnParseContext rval(*this);
  rval._startIndex = count;
  return rval;
}
void FnParseContext::dump(const std::string dumpid) const{
  printf( "FPC<%p:%s> idx<%zd>\n", this, dumpid.c_str(), _startIndex );
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx::parser
/////////////////////////////////////////////////////////////////////////////////////////////////
#endif