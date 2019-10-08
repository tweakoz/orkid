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

void ShaderDataNode::parse(ScannerView view) {
  DecoBlockNode::parse(view);

  ////////////////////////

  size_t inumdecos = view.numBlockDecorators();
  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok = view.blockDecorator(ideco);

  }
  ////////////////////////

  size_t ist = view._start + 1;
  size_t ien = view._end - 1;
  size_t i = ist;
  bool done = false;

  while (false == done) {
    const Token *dt_tok = view.token(i);
    const Token *nam_tok = view.token(i+1);

    bool is_endline = (dt_tok->text == "\n");
    if (is_endline) {
      i++;
    } else {
      auto it = _dupenamecheck.find(nam_tok->text);
      assert(
          it ==
          _dupenamecheck.end()); // make sure there are no duplicate uniforms

      _dupenamecheck.insert(nam_tok->text);

      bool typeok = _container->validateTypeName(dt_tok->text);
      bool nameok = _container->validateMemberName(nam_tok->text);
      assert(typeok);
      assert(nameok);

      auto unidecl = new UniformDeclNode;
      unidecl->_name = nam_tok->text;
      unidecl->_typeName = dt_tok->text;
      bool is_array = false;
      if (view.token(i + 3)->text == "[") {
        assert(view.token(i + 5)->text == "]");
        unidecl->_arraySize = atoi(view.token(i + 4)->text.c_str());
        is_array = true;
      }

      i += is_array ? 6 : 3;

    }
    done = (i >= ien);
    printf("ni<%d> ien<%d> done<%d>\n", int(i), int(ien), int(done));
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

UniformSet* UniformSetNode::generate(Container* outcon) const {

    assert(_blocktype == "uniform_set");

    UniformSet* uset = new UniformSet;
    auto pret = new UniformSet;
    uset->_name = _name;

    for( auto item : _uniformdecls ){
      Uniform *puni = outcon->MergeUniform(item->_name);
      puni->_typeName = item->_typeName;
      puni->_arraySize = item->_arraySize;
      uset->_uniforms[item->_name] = puni;
    }
    return uset;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

UniformBlock* UniformBlockNode::generate(Container* outcon) const {

    assert(_blocktype == "uniform_block");

    UniformBlock* ublk = new UniformBlock;
    auto pret = new UniformSet;
    ublk->_name = _name;

    for( auto item : _uniformdecls ){
      auto puni = new Uniform(item->_name);
      puni->_typeName = item->_typeName;
      puni->_arraySize = item->_arraySize;
      ublk->_subuniforms[item->_name] = puni;
    }
    return ublk;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
