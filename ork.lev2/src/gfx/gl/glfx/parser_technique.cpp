////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>
#include <ork/util/crc.h>
#include "../gl.h"
#include "glslfxi_parser.h"

namespace ork::lev2::glslfx {

///////////////////////////////////////////////////////////////////////////////

void TechniqueNode::parse(const ScannerView& view) {
  DecoBlockNode::parse(view);

  int ist = view._start + 1;
  int ien = view._end - 1;

  for (int i = ist; i <= ien;) {
    const Token* vt_tok = view.token(i);
    // printf( "  ParseFxTechnique Tok<%s>\n", vt_tok.text.c_str() );
    if (vt_tok->text == "fxconfig") {
      _fxconfig = view.token(i + 2)->text;
      i += 4;
    } else if (vt_tok->text == "pass") {
      // printf( "parsing pass at i<%d>\n", i );
      // i is in view space, we need the globspace index to
      //  start the pass parse
      auto passnode         = new PassNode(_container);
      int globspac_passtoki = view.globalTokenIndex(i);
      ScannerView subview(view._scanner, view._filter);
      subview.scanBlock(globspac_passtoki);
      passnode->parse(subview, globspac_passtoki);
      _passNodes[passnode->_name] = passnode;
      i += subview.numTokens();
    } else if (vt_tok->text == "\n") {
      i++;
    } else {
      OrkAssert(false);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

int PassNode::parse(const ScannerView& view, int istart) {
  NamedBlockNode::parse(view);
  
  int ist = view._start + 1;
  int ien = view._end - 1;

  for (size_t i = ist; i <= ien;) {
    const auto& item_key   = view.token(i)->text;
    const auto& item_value = view.token(i + 2)->text;
    if (item_key == "vertex_shader")
      _vertexshader = item_value;
    else if (item_key == "tessctrl_shader")
      _tessctrlshader = item_value;
    else if (item_key == "tesseval_shader")
      _tessevalshader = item_value;
    else if (item_key == "geometry_shader")
      _geometryshader = item_value;
    else if (item_key == "fragment_shader")
      _fragmentshader = item_value;
    else if (item_key == "state_block")
      _stateblock = item_value;
#if defined(ENABLE_NVMESH_SHADERS)
    else if (item_key == "nvtask_shader")
      _nvtaskshader = item_value;
    else if (item_key == "nvmesh_shader")
      _nvmeshshader = item_value;
#endif
    else {
      OrkAssert(false);
    }
    i += (item_key == "\n") ? 1 : 4;
  }
  /////////////////////////////////////////////
  return view.blockEnd() + 1;
}

///////////////////////////////////////////////////////////////////////////////

void TechniqueNode::generate(shaderbuilder::BackEnd& backend) const {
  Technique* ptek = new Technique(_name);
  for (auto item : _passNodes) {
    item.second->generate(backend);
    auto pass = backend._statemap["pass"].Get<Pass*>();
    ptek->addPass(pass);
  }
  backend._container->addTechnique(ptek);
}

///////////////////////////////////////////////////////////////////////////////

void PassNode::generate(shaderbuilder::BackEnd& backend) const {
  Pass* ppass = new Pass(_name);
  auto c = backend._container;
  backend._statemap["pass"].Set<Pass*>(ppass);
  /////////////////////////////////////////////////////////////
  // VTG pipe
  /////////////////////////////////////////////////////////////
  if (_vertexshader != "") {
    auto pshader = c->vertexShader(_vertexshader);
    OrkAssert(pshader != nullptr);
    auto& primvtg         = ppass->_primpipe.Make<PrimPipelineVTG>();
    primvtg._vertexShader = pshader;
  }
  if (_tessctrlshader != "") {
    auto pshader = c->tessCtrlShader(_tessctrlshader);
    OrkAssert(pshader != nullptr);
    auto& primvtg           = ppass->_primpipe.Get<PrimPipelineVTG>();
    primvtg._tessCtrlShader = pshader;
  }
  if (_tessevalshader != "") {
    auto pshader = c->tessEvalShader(_tessevalshader);
    OrkAssert(pshader != nullptr);
    auto& primvtg           = ppass->_primpipe.Get<PrimPipelineVTG>();
    primvtg._tessEvalShader = pshader;
  }
  if (_geometryshader != "") {
    auto pshader = c->geometryShader(_geometryshader);
    OrkAssert(pshader != nullptr);
    auto& primvtg           = ppass->_primpipe.Get<PrimPipelineVTG>();
    primvtg._geometryShader = pshader;
  }
  /////////////////////////////////////////////////////////////
  // NVTM pipe
  /////////////////////////////////////////////////////////////
  #if defined(ENABLE_NVMESH_SHADERS)
  if (_nvtaskshader != "") {
    auto pshader = c->nvTaskShader(_nvtaskshader);
    OrkAssert(pshader != nullptr);
    auto& primnvmt         = ppass->_primpipe.Make<PrimPipelineNVTM>();
    primnvmt._nvTaskShader = pshader;
  }
  if (_nvmeshshader != "") {
    auto pshader = c->nvMeshShader(_nvmeshshader);
    OrkAssert(pshader != nullptr);
    auto& primnvmt = ppass->_primpipe.IsA<PrimPipelineNVTM>() ? ppass->_primpipe.Get<PrimPipelineNVTM>()
                                                              : ppass->_primpipe.Make<PrimPipelineNVTM>();
    primnvmt._nvMeshShader = pshader;
  }
  #endif
  /////////////////////////////////////////////////////////////
  // do frag last so we already know pipe type
  /////////////////////////////////////////////////////////////
  if (_fragmentshader != "") {
    auto pshader = c->fragmentShader(_fragmentshader);
    OrkAssert(pshader != nullptr);
    if (auto as_vtg = ppass->_primpipe.TryAs<PrimPipelineVTG>())
      as_vtg.value()._fragmentShader = pshader;
    #if defined(ENABLE_NVMESH_SHADERS)
    else if (auto as_nvtm = ppass->_primpipe.TryAs<PrimPipelineNVTM>())
      as_nvtm.value()._fragmentShader = pshader;
    #endif
  }
  /////////////////////////////////////////////////////////////
  if (_stateblock != "") {
    StateBlock* psb = c->GetStateBlock(_stateblock);
    OrkAssert(psb != nullptr);
    ppass->_stateBlock = psb;
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
