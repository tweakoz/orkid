////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

UniformBlock *GlSlFxParser::parseUniformBlock() {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(itokidx);

  const std::string BlockType = v.token(v._blockType)->text;

  assert(BlockType == "uniform_block");

  ////////////////////////

  auto pret = new UniformBlock;
  pret->_name = v.blockName();
  ////////////////////////

  size_t inumdecos = v.numBlockDecorators();

  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok = v.blockDecorator(ideco);
  }

  ////////////////////////

  size_t ist = v._start + 1;
  size_t ien = v._end - 1;
  size_t i = ist;
  bool done = false;

  while (false == done) {
    const Token *dt_tok = v.token(i);
    const Token *nam_tok = v.token(i + 1);

    bool is_endline = (dt_tok->text == "\n");

    if (is_endline) {
      i++;
    } else {

      auto puni = new Uniform(nam_tok->text);
      puni->_typeName = dt_tok->text;
      pret->_subuniforms[nam_tok->text] = puni;
      printf("uniname<%s> typename<%s>\n", nam_tok->text.c_str(),
             puni->_typeName.c_str());

      bool is_array = false;
      if (v.token(i + 2)->text == "[") {
        assert(v.token(i + 4)->text == "]");
        puni->_arraySize = atoi(v.token(i + 3)->text.c_str());
        printf("uniname<%s> typename<%s> arraysize<%d>\n",
               nam_tok->text.c_str(), puni->_typeName.c_str(),
               puni->_arraySize);
        is_array = true;
      }

      i += is_array ? 6 : 3;
    }

    done = (i >= ien);
    printf("ni<%d> ien<%d> done<%d> dt_tok<%s>\n", int(i), int(ien), int(done),
           dt_tok->text.c_str());
  }
  itokidx = v.blockEnd() + 1;
  return pret;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

UniformSet *GlSlFxParser::parseUniformSet() {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(itokidx);

  const std::string BlockType = v.token(v._blockType)->text;

  assert(BlockType == "uniform_set");

  ////////////////////////

  auto pret = new UniformSet;
  pret->_name = v.blockName();
  ////////////////////////

  size_t inumdecos = v.numBlockDecorators();

  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok = v.blockDecorator(ideco);
  }

  ////////////////////////

  size_t ist = v._start + 1;
  size_t ien = v._end - 1;
  size_t i = ist;
  bool done = false;

  while (false == done) {
    const Token *vt_tok = v.token(i);
    const Token *dt_tok = v.token(i + 1);
    const Token *nam_tok = v.token(i + 2);

    // printf( "oi<%d> done<%d> ist<%d> ien<%d>\n", int(i), int(done),
    // int(ist),int(ien) );

    bool is_endline = (vt_tok->text == "\n");
    if (is_endline) {
      i++;
    } else if (vt_tok->text == "uniform") {
      auto it = pret->_uniforms.find(nam_tok->text);
      assert(
          it ==
          pret->_uniforms.end()); // make sure there are no duplicate uniforms

      Uniform *puni = mpContainer->MergeUniform(nam_tok->text);
      puni->_typeName = dt_tok->text;
      pret->_uniforms[nam_tok->text] = puni;
      printf("uniname<%s> typename<%s>\n", nam_tok->text.c_str(),
             puni->_typeName.c_str());

      bool is_array = false;

      if (v.token(i + 3)->text == "[") {
        assert(v.token(i + 5)->text == "]");
        puni->_arraySize = atoi(v.token(i + 4)->text.c_str());
        printf("uniname<%s> typename<%s> arraysize<%d>\n",
               nam_tok->text.c_str(), puni->_typeName.c_str(),
               puni->_arraySize);

        is_array = true;
      }

      i += is_array ? 7 : 4;

    } else {
      assert(false);
    }
    done = (i >= ien);
    printf("ni<%d> ien<%d> done<%d>\n", int(i), int(ien), int(done));
  }
  itokidx = v.blockEnd() + 1;
  return pret;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
