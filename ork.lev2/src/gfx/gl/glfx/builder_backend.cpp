////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>

namespace ork::lev2::glslfx::shaderbuilder {
////////////////////////////////////////////////////////////////

BackEnd::BackEnd(parser::parser_constptr_t p, 
                 rootcontainer_ptr_t c) {
  _parser   = p;
  _container = c;
}

////////////////////////////////////////////////////////////////

bool BackEnd::generate() {

  auto program = _parser->_program;
  
  bool ok = true;
  try {
    program->generate_all(*this);
  } catch (...) {
    ok = false;
  }
  return ok;
}

///////////////////////////////////////////////////////////
void BackEnd::validateTypeName(const std::string typeName) const {
  bool typeok = isTypeName(typeName);
  if (false == typeok) {
    deco::printf(fvec3::Red(), "type not found: ");
    deco::printf(fvec3::White(), "%s\n", typeName.c_str());
    assert(false);
  }
}
bool BackEnd::isTypeName(const std::string typeName) const{
  auto it = _validTypeNames.find(typeName);
  return (it != _validTypeNames.end());
}

////////////////////////////////////////////////////////////////

void BackendCodeGen::beginLine() {
  if (_curline.length())
    endLine();

  int lineno = _lines.size();
  _curline   = FormatString("/*%03d*/ ", lineno+1);
  for (int i = 0; i < _indentLevel; i++)
    _curline += "  ";
}

////////////////////////////////////////////////////////////////

void BackendCodeGen::endLine() {
  _lines.push_back(_curline);
  _curline = "";
}

////////////////////////////////////////////////////////////////

void BackendCodeGen::incIndent() { _indentLevel++; }

////////////////////////////////////////////////////////////////

void BackendCodeGen::decIndent() { _indentLevel--; }

////////////////////////////////////////////////////////////////

void BackendCodeGen::format(const char* fmt, ...) {
  constexpr int kmaxlen = 1024;
  char buffer[kmaxlen];
  va_list argp;
  va_start(argp, fmt);
  vsnprintf(&buffer[0], kmaxlen, fmt, argp);
  va_end(argp);
  output(buffer);
}

////////////////////////////////////////////////////////////////

void BackendCodeGen::output(std::string str) { _curline += str; }

////////////////////////////////////////////////////////////////

void BackendCodeGen::formatLine(const char* fmt, ...) {
  constexpr int kmaxlen = 1024;
  char buffer[kmaxlen];
  va_list argp;
  va_start(argp, fmt);
  vsnprintf(&buffer[0], kmaxlen, fmt, argp);
  va_end(argp);

  beginLine();
  output(buffer);
  endLine();
}

////////////////////////////////////////////////////////////////

std::string BackendCodeGen::flush() {
  std::string rval;
  for (auto l : _lines)
    rval += l + "\n";
  _curline = "";
  _lines.clear();
  return rval;
}

////////////////////////////////////////////////////////////////
}