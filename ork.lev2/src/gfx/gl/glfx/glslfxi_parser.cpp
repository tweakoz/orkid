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

const std::map<std::string, int> GlSlFxParser::gattrsorter = {
    {"POSITION", 0},  {"NORMAL", 1},      {"COLOR0", 2},       {"COLOR1", 3},
    {"TEXCOORD0", 4}, {"TEXCOORD0", 5},   {"TEXCOORD1", 6},    {"TEXCOORD2", 7},
    {"TEXCOORD3", 8}, {"BONEINDICES", 9}, {"BONEWEIGHTS", 10},
};

///////////////////////////////////////////////////////////
GlSlFxParser::GlSlFxParser(const AssetPath &pth, Scanner &s)
    : mPath(pth), scanner(s), mpContainer(nullptr) {}
///////////////////////////////////////////////////////////
bool GlSlFxParser::IsTokenOneOfTheBlockTypes(const Token &tok) {
  std::regex regex_block(token_regex);
  return std::regex_match(tok.text, regex_block);
}
///////////////////////////////////////////////////////////
Config *GlSlFxParser::ParseFxConfig() {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(itokidx);

  Config *pcfg = new Config;
  pcfg->mName = v.blockName();

  int ist = v._start + 1;
  int ien = v._end - 1;

  std::vector<std::string> imports;
  for (size_t i = ist; i <= ien;) {
    const Token *vt_tok = v.token(i);

    // printf( "vt_tok<%s>\n", vt_tok->text.c_str() );

    if (vt_tok->text == "import") {
      const Token *impnam = v.token(i + 1);
      std::string p = impnam->text.substr(1, impnam->text.length() - 2);
      imports.push_back(p);

      i += 3;
    } else
      i++;
  }

  itokidx = v.blockEnd() + 1;

  for (const auto &imp : imports) {
    Scanner scanner2;

    file::Path::NameType a, b;
    mPath.Split(a, b, ':');

    ork::FixedString<256> fxs;
    fxs.format("%s://%s", a.c_str(), imp.c_str());
    file::Path imppath = fxs.c_str();
    // assert(false);

    // printf( "impnam<%s> a<%s> b<%s> imppath<%s>\n", imp.c_str(), a.c_str(),
    // b.c_str(), imppath.c_str() );
    ///////////////////////////////////
    File fx_file(imppath.c_str(), EFM_READ);
    OrkAssert(fx_file.IsOpen());
    EFileErrCode eFileErr = fx_file.GetLength(scanner2.ifilelen);
    OrkAssert(scanner2.ifilelen < scanner2.kmaxfxblen);
    eFileErr = fx_file.Read(scanner2.fxbuffer, scanner2.ifilelen);
    scanner2.fxbuffer[scanner2.ifilelen] = 0;
    ///////////////////////////////////
    scanner2.Scan();

    const auto &stoks = scanner2.tokens;
    auto &dtoks = scanner.tokens;

    dtoks.insert(dtoks.begin() + itokidx, stoks.begin(), stoks.end());
  }

  return pcfg;
}


///////////////////////////////////////////////////////////
StateBlock *GlSlFxParser::ParseFxStateBlock() {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(itokidx);

  StateBlock *psb = new StateBlock;
  psb->mName = v.blockName();
  mpContainer->addStateBlock(psb);
  //////////////////////

  auto &apptors = psb->mApplicators;

  //////////////////////

  size_t inumdecos = v.numBlockDecorators();

  assert(inumdecos < 2);

  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok = v.blockDecorator(ideco);
    StateBlock *ppar = mpContainer->GetStateBlock(ptok->text);
    OrkAssert(ppar != nullptr);
    psb->mApplicators = ppar->mApplicators;
  }

  //////////////////////

  int ist = v._start + 1;
  int ien = v._end - 1;

  for (size_t i = ist; i <= ien;) {

    const Token *vt_tok = v.token(i);
    // printf( "  ParseFxStateBlock Tok<%s>\n", vt_tok.text.c_str() );
    if (vt_tok->text == "inherits") {
      const Token *parent_tok = v.token(i + 1);
      StateBlock *ppar = mpContainer->GetStateBlock(parent_tok->text);
      OrkAssert(ppar != nullptr);
      psb->mApplicators = ppar->mApplicators;
      i += 3;
    } else if (vt_tok->text == "CullTest") {
      const std::string &mode = v.token(i + 2)->text;
      if (mode == "OFF")
        psb->addStateFn(
            [=](GfxTarget *t) { t->RSI()->SetCullTest(lev2::ECULLTEST_OFF); });
      else if (mode == "PASS_FRONT")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetCullTest(lev2::ECULLTEST_PASS_FRONT);
        });
      else if (mode == "PASS_BACK")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetCullTest(lev2::ECULLTEST_PASS_BACK);
        });

      i += 4;
    } else if (vt_tok->text == "DepthMask") {
      const std::string &mode = v.token(i + 2)->text;
      bool bena = (mode == "true");
      psb->addStateFn([=](GfxTarget *t) { t->RSI()->SetZWriteMask(bena); });
      // printf( "DepthMask<%d>\n", int(bena) );
      i += 4;
    } else if (vt_tok->text == "DepthTest") {
      const std::string &mode = v.token(i + 2)->text;
      if (mode == "OFF")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetDepthTest(lev2::EDEPTHTEST_OFF);
        });
      else if (mode == "LESS")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetDepthTest(lev2::EDEPTHTEST_LESS);
        });
      else if (mode == "LEQUALS")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetDepthTest(lev2::EDEPTHTEST_LEQUALS);
        });
      else if (mode == "GREATER")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetDepthTest(lev2::EDEPTHTEST_GREATER);
        });
      else if (mode == "GEQUALS")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetDepthTest(lev2::EDEPTHTEST_GEQUALS);
        });
      else if (mode == "EQUALS")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetDepthTest(lev2::EDEPTHTEST_EQUALS);
        });
      i += 4;
    } else if (vt_tok->text == "BlendMode") {
      const std::string &mode = v.token(i + 2)->text;
      if (mode == "ADDITIVE")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetBlending(lev2::EBLENDING_ADDITIVE);
        });
      else if (mode == "ALPHA_ADDITIVE")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetBlending(lev2::EBLENDING_ALPHA_ADDITIVE);
        });
      else if (mode == "ALPHA")
        psb->addStateFn([=](GfxTarget *t) {
          t->RSI()->SetBlending(lev2::EBLENDING_ALPHA);
        });
      i += 4;
    } else if (vt_tok->text == "\n") {
      i++;
    } else {
      OrkAssert(false);
    }
  }
  //////////////////////
  itokidx = v.blockEnd() + 1;
  return psb;
}
///////////////////////////////////////////////////////////
LibBlock *GlSlFxParser::ParseLibraryBlock() {
  auto pret = new LibBlock(scanner);
  pret->mView->scanBlock(itokidx);
  pret->mName = scanner.tokens[itokidx + 1].text;
  itokidx = pret->mView->blockEnd() + 1;
  return pret;
}

///////////////////////////////////////////////////////////
Technique *GlSlFxParser::ParseFxTechnique() {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(itokidx);

  // int iend = FindEndOfBlock( itokidx+1, itokidx+2 );
  // const token& etok = scanner.tokens[iend+1];
  // printf( "ParseFxTechnique Eob<%d> Next<%s>\n", iend, etok.text.c_str() );

  std::string tekname = v.blockName();

  Technique *ptek = new Technique(tekname);
  ////////////////////////////////////////
  // OrkAssert( scanner.tokens[itokidx+2].text == "{" );
  ////////////////////////////////////////

  int ist = v._start + 1;
  int ien = v._end - 1;

  for (int i = ist; i <= ien;) {
    const Token *vt_tok = v.token(i);
    // printf( "  ParseFxTechnique Tok<%s>\n", vt_tok.text.c_str() );
    if (vt_tok->text == "fxconfig") {
      i += 4;
    } else if (vt_tok->text == "pass") {
      // printf( "parsing pass at i<%d>\n", i );
      // i is in view space, we need the globspace index to
      //  start the pass parse
      int globspac_passtoki = v.tokenIndex(i);
      i = ParseFxPass(globspac_passtoki, ptek);
    } else if (vt_tok->text == "\n") {
      i++;
    } else {
      OrkAssert(false);
    }
  }
  ////////////////////////////////////////
  itokidx = v.blockEnd() + 1;
  return ptek;
}
///////////////////////////////////////////////////////////
int GlSlFxParser::ParseFxPass(int istart, Technique *ptek) {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(istart);

  std::string name = v.blockName();
  // printf( "ParseFxPass Name<%s> Eob<%d> Next<%s>\n", name.c_str(), iend,
  // etok.text.c_str() );
  Pass *ppass = new Pass(name);
  ////////////////////////////////////////
  // OrkAssert( scanner.tokens[istart+2].text == "{" );
  /////////////////////////////////////////////

  int ist = v._start + 1;
  int ien = v._end - 1;

  for (size_t i = ist; i <= ien;) {
    const Token *vt_tok = v.token(i);
    // printf( "  ParseFxPass Tok<%s>\n", vt_tok->text.c_str() );

    if (vt_tok->text == "vertex_shader") {
      std::string vsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->vertexShader(vsnam);
      OrkAssert(pshader != nullptr);
      auto &primvtg = ppass->_primpipe.Make<PrimPipelineVTG>();
      primvtg._vertexShader = pshader;
      i += 4;
    } else if (vt_tok->text == "tessctrl_shader") {
      std::string fsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->tessCtrlShader(fsnam);
      OrkAssert(pshader != nullptr);
      auto &primvtg = ppass->_primpipe.Get<PrimPipelineVTG>();
      primvtg._tessCtrlShader = pshader;
      i += 4;
    } else if (vt_tok->text == "tesseval_shader") {
      std::string fsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->tessEvalShader(fsnam);
      OrkAssert(pshader != nullptr);
      auto &primvtg = ppass->_primpipe.Get<PrimPipelineVTG>();
      primvtg._tessEvalShader = pshader;
      i += 4;
    } else if (vt_tok->text == "geometry_shader") {
      std::string fsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->geometryShader(fsnam);
      OrkAssert(pshader != nullptr);
      auto &primvtg = ppass->_primpipe.Get<PrimPipelineVTG>();
      primvtg._geometryShader = pshader;
      i += 4;
    } else if (vt_tok->text == "fragment_shader") {
      std::string fsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->fragmentShader(fsnam);
      OrkAssert(pshader != nullptr);
      auto &primvtg = ppass->_primpipe.Get<PrimPipelineVTG>();
      primvtg._fragmentShader = pshader;
      i += 4;
    }
#if defined(ENABLE_NVMESH_SHADERS)
    else if (vt_tok->text == "nvtask_shader") {
      std::string fsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->nvTaskShader(fsnam);
      OrkAssert(pshader != nullptr);
      auto &primnvmt = ppass->_primpipe.Get<PrimPipelineNVMT>();
      primnvmt._nvTaskShader = pshader;
      i += 4;
    } else if (vt_tok->text == "nvmesh_shader") {
      std::string fsnam = v.token(i + 2)->text;
      auto pshader = mpContainer->nvMeshShader(fsnam);
      OrkAssert(pshader != nullptr);
      auto &primnvmt = ppass->_primpipe.Get<PrimPipelineNVMT>();
      primnvmt._nvMeshShader = pshader;
      i += 4;
    }
#endif
    else if (vt_tok->text == "state_block") {
      std::string sbnam = v.token(i + 2)->text;
      StateBlock *psb = mpContainer->GetStateBlock(sbnam);
      OrkAssert(psb != nullptr);
      ppass->_stateBlock = psb;
      i += 4;
    } else if (vt_tok->text == "\n") {
      i++;
    } else {
      OrkAssert(false);
    }
  }
  /////////////////////////////////////////////
  /////////////////////////////////////////////
  ptek->addPass(ppass);
  /////////////////////////////////////////////
  return v.blockEnd() + 1;
}
///////////////////////////////////////////////////////////
void GlSlFxParser::DumpAllTokens() {
  size_t itokidx = 0;
  const auto &tokens = scanner.tokens;
  while (itokidx < tokens.size()) {
    const Token &tok = tokens[itokidx++];
    // printf( "tok<%d> <%s>\n", itokidx, tok.text.c_str() );
  }
}
///////////////////////////////////////////////////////////
Container *GlSlFxParser::Parse(const std::string &fxname) {
  const auto &tokens = scanner.tokens;

  // printf( "NumTokens<%d>\n", int(tokens.size()) );

  mpContainer = new Container(fxname.c_str());
  bool bOK = true;

  itokidx = 0;

  while (itokidx < tokens.size()) {
    const Token &tok = tokens[itokidx];
    // printf( "token<%d> iline<%d> col<%d> text<%s>\n", itokidx, tok.iline+1,
    // tok.icol+1, tok.text.c_str() );

    if (tok.text == "\n") {
      itokidx++;
    } else if (tok.text == "fxconfig") {
      Config *pconfig = ParseFxConfig();
      mpContainer->addConfig(pconfig);
    } else if (tok.text == "libblock") {
      auto lb = ParseLibraryBlock();
      mpContainer->addLibBlock(lb);
    } else if (tok.text == "uniform_set") {
      auto pif = parseUniformSet();
      mpContainer->addUniformSet(pif);
    } else if (tok.text == "uniform_block") {
      auto block = parseUniformBlock();
      mpContainer->addUniformBlock(block);
    } else if (tok.text == "vertex_interface") {
      StreamInterface *pif = ParseFxInterface(GL_VERTEX_SHADER);
      mpContainer->addVertexInterface(pif);
    } else if (tok.text == "tessctrl_interface") {
      StreamInterface *pif = ParseFxInterface(GL_TESS_CONTROL_SHADER);
      mpContainer->addTessCtrlInterface(pif);
    } else if (tok.text == "tesseval_interface") {
      StreamInterface *pif = ParseFxInterface(GL_TESS_EVALUATION_SHADER);
      mpContainer->addTessEvalInterface(pif);
    } else if (tok.text == "geometry_interface") {
      StreamInterface *pif = ParseFxInterface(GL_GEOMETRY_SHADER);
      mpContainer->addGeometryInterface(pif);
    } else if (tok.text == "fragment_interface") {
      StreamInterface *pif = ParseFxInterface(GL_FRAGMENT_SHADER);
      mpContainer->addFragmentInterface(pif);
    } else if (tok.text == "state_block") {
      StateBlock *psblock = ParseFxStateBlock();
      mpContainer->addStateBlock(psblock);
    } else if (tok.text == "vertex_shader") {
      ShaderVtx *pshader = ParseFxVertexShader();
      mpContainer->addVertexShader(pshader);
    } else if (tok.text == "tessctrl_shader") {
      ShaderTsC *pshader = ParseFxTessCtrlShader();
      mpContainer->addTessCtrlShader(pshader);
    } else if (tok.text == "tesseval_shader") {
      ShaderTsE *pshader = ParseFxTessEvalShader();
      mpContainer->addTessEvalShader(pshader);
    } else if (tok.text == "geometry_shader") {
      ShaderGeo *pshader = ParseFxGeometryShader();
      mpContainer->addGeometryShader(pshader);
    } else if (tok.text == "fragment_shader") {
      ShaderFrg *pshader = ParseFxFragmentShader();
      mpContainer->addFragmentShader(pshader);
    }
#if defined(ENABLE_NVMESH_SHADERS)
    else if (tok.text == "nvtask_shader") {
      ShaderNvTask *pshader = ParseFxNvTaskShader();
      mpContainer->addNvTaskShader(pshader);
    } else if (tok.text == "nvmesh_shader") {
      ShaderNvMesh *pshader = ParseFxNvMeshShader();
      mpContainer->addNvMeshShader(pshader);
    }
#endif
    else if (tok.text == "technique") {
      Technique *ptek = ParseFxTechnique();
      mpContainer->addTechnique(ptek);
    } else {
      printf("Unknown Token<%s>\n", tok.text.c_str());
      OrkAssert(false);
    }
  }
  if (false == bOK) {
    delete mpContainer;
    mpContainer = nullptr;
  }
  return mpContainer;
}

LibBlock::LibBlock(const Scanner &s) : mFilter(nullptr), mView(nullptr) {
  mFilter = new ScanViewFilter();
  mView = new ScannerView(s, *mFilter);
}

//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////

Container *LoadFxFromFile(const AssetPath &pth) {
  Scanner scanner;
  GlSlFxParser parser(pth, scanner);
  ///////////////////////////////////
  File fx_file(pth, EFM_READ);
  OrkAssert(fx_file.IsOpen());
  EFileErrCode eFileErr = fx_file.GetLength(scanner.ifilelen);
  OrkAssert(scanner.ifilelen < scanner.kmaxfxblen);
  eFileErr = fx_file.Read(scanner.fxbuffer, scanner.ifilelen);
  scanner.fxbuffer[scanner.ifilelen] = 0;
  ///////////////////////////////////
  scanner.Scan();
  Container *pcont = parser.Parse(pth.c_str());
  return pcont;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
