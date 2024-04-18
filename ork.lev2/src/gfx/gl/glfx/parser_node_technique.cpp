////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crc.h>
#include "../gl.h"
#include "glslfxi_parser.h"

namespace ork::lev2::glslfx::parser {

///////////////////////////////////////////////////////////////////////////////

void TechniqueNode::parse(GlSlFxParser* parser, const ScannerView& view) {
  DecoBlockNode::parse(parser, view);

  int ist = view._start + 1;
  int ien = view._end - 1;

  for (int i = ist; i <= ien;) {
    const Token* vt_tok = view.token(i);
    // printf( "  ParseFxTechnique Tok<%s>\n", vt_tok.text.c_str() );
    ////////////////////////////////////////////////////
    // short form vertex/fragment shader pass
    ////////////////////////////////////////////////////
    auto tekname = this->_name;
    if (vt_tok->text == "vf_pass") {
      OrkAssert(view.token(i + 1)->text == "=");
      OrkAssert(view.token(i + 2)->text == "{");
      OrkAssert(view.token(i + 4)->text == ",");
      OrkAssert(view.token(i + 6)->text == ",");
      OrkAssert(view.token(i + 8)->text == "}");
      auto passnode             = new PassNode(this);
      size_t numpasses          = _passNodes.size();
      auto passname             = FormatString("%s_p%zu", tekname.c_str(), numpasses);
      passnode->_name           = passname;
      _passNodes[passname]      = passnode;
      passnode->_vertexshader   = view.token(i + 3)->text;
      passnode->_fragmentshader = view.token(i + 5)->text;
      passnode->_stateblock     = view.token(i + 7)->text;
      //printf("vtxs<%s>\n", passnode->_vertexshader.c_str());
      //printf("frgs<%s>\n", passnode->_fragmentshader.c_str());
      //printf("sblk<%s>\n", passnode->_stateblock.c_str());
      i += 9;
    }
    ////////////////////////////////////////////////////
    else if (vt_tok->text == "fxconfig") {
      _fxconfig = view.token(i + 2)->text;
      i += 4;
    }
    ////////////////////////////////////////////////////
    else if (vt_tok->text == "pass") {
      // printf( "parsing pass at i<%d>\n", i );
      // i is in view space, we need the globspace index to
      //  start the pass parse
      auto passnode         = new PassNode(this);
      int globspac_passtoki = view.globalTokenIndex(i);
      ScannerView subview(view._scanner, view._filter);
      subview.scanBlock(globspac_passtoki);
      passnode->parse(parser,subview, globspac_passtoki);
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

int PassNode::parse(GlSlFxParser* parser, const ScannerView& view, int istart) {
  NamedBlockNode::parse(parser, view);

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

void TechniqueNode::_generate2(shaderbuilder::BackEnd& backend) const {
  Technique* ptek = new Technique(_name);
  for (auto item : _passNodes) {
    item.second->_generate2(backend);
    auto pass = backend._statemap["pass"].get<Pass*>();
    ptek->addPass(pass);
  }
  backend._container->addTechnique(ptek);
}

///////////////////////////////////////////////////////////////////////////////

void PassNode::_generate2(shaderbuilder::BackEnd& backend) const {
  Pass* ppass = new Pass(_name);
  auto c      = backend._container;
  backend._statemap["pass"].set<Pass*>(ppass);
  /////////////////////////////////////////////////////////////
  // VTG pipe
  /////////////////////////////////////////////////////////////
  if (_vertexshader != "") {
    auto pshader = c->vertexShader(_vertexshader);
    if(pshader==nullptr){
      deco::printf(fvec3::Red(), "vertex shader <%s> not found!\n", _vertexshader.c_str());
    }
    OrkAssert(pshader != nullptr);
    auto& primvtg         = ppass->_primpipe.make<PrimPipelineVTG>();
    primvtg._vertexShader = pshader;
  }
  if (_tessctrlshader != "") {
    auto pshader = c->tessCtrlShader(_tessctrlshader);
    if(pshader==nullptr){
      deco::printf(fvec3::Red(), "tessctrl shader <%s> not found!\n", _vertexshader.c_str());
    }
    OrkAssert(pshader != nullptr);
    auto& primvtg           = ppass->_primpipe.get<PrimPipelineVTG>();
    primvtg._tessCtrlShader = pshader;
  }
  if (_tessevalshader != "") {
    auto pshader = c->tessEvalShader(_tessevalshader);
    if(pshader==nullptr){
      deco::printf(fvec3::Red(), "tesseval shader <%s> not found!\n", _vertexshader.c_str());
    }
    OrkAssert(pshader != nullptr);
    auto& primvtg           = ppass->_primpipe.get<PrimPipelineVTG>();
    primvtg._tessEvalShader = pshader;
  }
  if (_geometryshader != "") {
    auto pshader = c->geometryShader(_geometryshader);
    if(pshader==nullptr){
      deco::printf(fvec3::Red(), "geometry shader <%s> not found!\n", _vertexshader.c_str());
    }
    OrkAssert(pshader != nullptr);
    auto& primvtg           = ppass->_primpipe.get<PrimPipelineVTG>();
    primvtg._geometryShader = pshader;
  }
/////////////////////////////////////////////////////////////
// NVTM pipe
/////////////////////////////////////////////////////////////
#if defined(ENABLE_NVMESH_SHADERS)
  if (_nvtaskshader != "") {
    auto pshader = c->nvTaskShader(_nvtaskshader);
    OrkAssert(pshader != nullptr);
    auto& primnvmt         = ppass->_primpipe.make<PrimPipelineNVTM>();
    primnvmt._nvTaskShader = pshader;
  }
  if (_nvmeshshader != "") {
    auto pshader = c->nvMeshShader(_nvmeshshader);
    OrkAssert(pshader != nullptr);
    auto& primnvmt = ppass->_primpipe.isA<PrimPipelineNVTM>() ? ppass->_primpipe.get<PrimPipelineNVTM>()
                                                              : ppass->_primpipe.make<PrimPipelineNVTM>();
    primnvmt._nvMeshShader = pshader;
  }
#endif
  /////////////////////////////////////////////////////////////
  // do frag last so we already know pipe type
  /////////////////////////////////////////////////////////////
  if (_fragmentshader != "") {
    auto pshader = c->fragmentShader(_fragmentshader);
    if (pshader == nullptr) {
      deco::printf(fvec3::Red(), "fragment shader not found!\n");
      deco::printf(
          fvec3::Red(), "  fragshad<%s> pass<%s> tek<%s>\n", _fragmentshader.c_str(), _name.c_str(), _techniqueNode->_name.c_str());
      OrkAssert(false);
    }
    if (auto as_vtg = ppass->_primpipe.tryAs<PrimPipelineVTG>())
      as_vtg.value()._fragmentShader = pshader;
#if defined(ENABLE_NVMESH_SHADERS)
    else if (auto as_nvtm = ppass->_primpipe.tryAs<PrimPipelineNVTM>())
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
} // namespace ork::lev2::glslfx::parser
