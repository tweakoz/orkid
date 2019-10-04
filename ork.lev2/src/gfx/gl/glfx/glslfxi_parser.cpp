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
#include "glslfxi_scanner.h"
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

static const std::map<std::string, int> gattrsorter = {
    {"POSITION", 0},  {"NORMAL", 1},      {"COLOR0", 2},       {"COLOR1", 3},
    {"TEXCOORD0", 4}, {"TEXCOORD0", 5},   {"TEXCOORD1", 6},    {"TEXCOORD2", 7},
    {"TEXCOORD3", 8}, {"BONEINDICES", 9}, {"BONEWEIGHTS", 10},
};

StreamInterface::StreamInterface() : mInterfaceType(GL_NONE), mGsPrimSize(0) {}

void StreamInterface::Inherit(const StreamInterface &par) {
  bool is_vtx = mInterfaceType == GL_VERTEX_SHADER;
  bool is_tec = mInterfaceType == GL_TESS_CONTROL_SHADER;
  bool is_tee = mInterfaceType == GL_TESS_EVALUATION_SHADER;
  bool is_geo = mInterfaceType == GL_GEOMETRY_SHADER;
  bool is_frg = mInterfaceType == GL_FRAGMENT_SHADER;

  bool par_is_vtx = par.mInterfaceType == GL_VERTEX_SHADER;
  bool par_is_tee = par.mInterfaceType == GL_TESS_EVALUATION_SHADER;

  bool types_match = mInterfaceType == par.mInterfaceType;
  bool geoinhvtx = is_geo && par_is_vtx;
  bool geoinhtee = is_geo && par_is_tee;
  bool inherit_ok = types_match | geoinhvtx | geoinhtee;

  assert(inherit_ok);

  ////////////////////////////////

  if (types_match) {
    for (const auto &item : par.mPreamble)
      mPreamble.push_back(item);

    for (const auto &ub : par._uniformSets)
      _uniformSets.insert(ub);
  }

  if (is_geo && (types_match || geoinhvtx || geoinhtee)) {
    // printf( "pre_inherit mGsPrimSize<%d>\n", mGsPrimSize );
    if (mGsPrimSize == 0 && par.mGsPrimSize != 0)
      mGsPrimSize = par.mGsPrimSize;
    // printf( "inherit mGsPrimSize<%d>\n", mGsPrimSize );
  }

  ////////////////////////////////
  // convert vertex out attrs
  // to geom in array attrs
  ////////////////////////////////

  bool conv_vtx_to_geo = (is_geo && par_is_vtx);
  bool conv_tee_to_geo = (is_geo && par_is_tee);
  bool conv_to_geo = conv_vtx_to_geo || conv_tee_to_geo;

  ////////////////////////////////

  for (const auto &a : par.mAttributes) {
    auto it = mAttributes.find(a.first);
    assert(it == mAttributes.end()); // make sure there are no duplicate attrs

    const Attribute *src = a.second;

    // printf( "Convert attributes conv_to_geo<%d>\n", int(conv_to_geo) );

    if (conv_to_geo) {
      if (src->mDirection == "out") {
        Attribute *cpy = new Attribute(src->mName, src->mSemantic);
        cpy->mTypeName = src->mTypeName;
        cpy->mDirection = "in";
        cpy->meType = src->meType;
        assert(mGsPrimSize != 0);
        cpy->mArraySize = mGsPrimSize;
        cpy->mLocation = int(mAttributes.size());
        cpy->mComment = "// (vtx/tee)->geo";

        mAttributes[a.first] = cpy;

        // printf( "copied (vtx/tee)->geo nam<%s> typ<%s>\n",
        // src->mName.c_str(), src->mTypeName.c_str() );
      }
    } else {
      Attribute *cpy = new Attribute(src->mName, src->mSemantic);
      cpy->mTypeName = src->mTypeName;
      cpy->mDirection = src->mDirection;
      cpy->meType = src->meType;

      cpy->mLocation = int(mAttributes.size());
      mAttributes[a.first] = cpy;
    }
  }
}

struct GlSlFxParser {
  int itokidx;
  Scanner &scanner;
  const AssetPath mPath;
  Container *mpContainer;

  ///////////////////////////////////////////////////////////
  GlSlFxParser(const AssetPath &pth, Scanner &s)
      : mPath(pth), scanner(s), mpContainer(nullptr) {}
  ///////////////////////////////////////////////////////////
  bool IsTokenOneOfTheBlockTypes(const Token &tok) {
    std::regex regex_block(token_regex);
    return std::regex_match(tok.text, regex_block);
  }
  ///////////////////////////////////////////////////////////
  Config *ParseFxConfig() {
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
  UniformBlock *parseUniformBlock() {
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
        puni->mTypeName = dt_tok->text;
        pret->_uniforms[nam_tok->text] = puni;
        printf("uniname<%s> typename<%s>\n", nam_tok->text.c_str(),
               puni->mTypeName.c_str());

        bool is_array = false;
        if (v.token(i + 2)->text == "[") {
          assert(v.token(i + 4)->text == "]");
          puni->mArraySize = atoi(v.token(i + 3)->text.c_str());
          printf("uniname<%s> typename<%s> arraysize<%d>\n",
                 nam_tok->text.c_str(), puni->mTypeName.c_str(),
                 puni->mArraySize);
          is_array = true;
        }

        i += is_array ? 6 : 3;
      }

      done = (i >= ien);
      printf("ni<%d> ien<%d> done<%d> dt_tok<%s>\n", int(i), int(ien),
             int(done), dt_tok->text.c_str());
    }
    assert(false);
    return pret;
  }
  ///////////////////////////////////////////////////////////
  UniformSet *parseUniformSet() {
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
        puni->mTypeName = dt_tok->text;
        pret->_uniforms[nam_tok->text] = puni;
        printf("uniname<%s> typename<%s>\n", nam_tok->text.c_str(),
               puni->mTypeName.c_str());

        bool is_array = false;

        if (v.token(i + 3)->text == "[") {
          assert(v.token(i + 5)->text == "]");
          puni->mArraySize = atoi(v.token(i + 4)->text.c_str());
          printf("uniname<%s> typename<%s> arraysize<%d>\n",
                 nam_tok->text.c_str(), puni->mTypeName.c_str(),
                 puni->mArraySize);

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
  ///////////////////////////////////////////////////////////
  StreamInterface *ParseFxInterface(GLenum iftype) {
    ScanViewRegex r("(\n)", true);
    ScannerView v(scanner, r);
    v.scanBlock(itokidx);

    ////////////////////////

    StreamInterface *psi = new StreamInterface;
    psi->mName = v.blockName();
    psi->mInterfaceType = iftype;

    ////////////////////////

    const std::string BlockType = v.token(v._blockType)->text;
    bool is_vtx = BlockType == "vertex_interface";
    bool is_geo = BlockType == "geometry_interface";

    /////////////////////////////
    // interface inheritance
    /////////////////////////////

    size_t inumdecos = v.numBlockDecorators();

    for (size_t ideco = 0; ideco < inumdecos; ideco++) {
      auto ptok = v.blockDecorator(ideco);

      auto it_ub = mpContainer->_uniformSets.find(ptok->text);

      if (it_ub != mpContainer->_uniformSets.end()) {
        psi->_uniformSets.insert(it_ub->second);
      } else if (is_vtx) {
        auto it_vi = mpContainer->_vertexInterfaces.find(ptok->text);
        assert(it_vi != mpContainer->_vertexInterfaces.end());
        psi->Inherit(*it_vi->second);
      } else if (is_geo) {
        auto it_fig = mpContainer->_geometryInterfaces.find(ptok->text);
        auto it_fiv = mpContainer->_vertexInterfaces.find(ptok->text);
        auto it_fie = mpContainer->_tessEvalInterfaces.find(ptok->text);
        bool is_geo = (it_fig != mpContainer->_geometryInterfaces.end());
        bool is_vtx = (it_fiv != mpContainer->_vertexInterfaces.end());
        bool is_tee = (it_fie != mpContainer->_tessEvalInterfaces.end());
        assert(is_geo || is_vtx || is_tee);

        auto par = is_geo ? it_fig->second
                          : is_vtx ? it_fiv->second
                                   : is_tee ? it_fie->second : nullptr;

        assert(par != nullptr);

        // printf( "iface<%s> inherit<%s:%p>\n",
        //	psi->mName.c_str(),
        //	ptok->text.c_str(), par );

        psi->Inherit(*par);
      } else {
        auto it_fi = mpContainer->_fragmentInterfaces.find(ptok->text);
        assert(it_fi != mpContainer->_fragmentInterfaces.end());
        psi->Inherit(*it_fi->second);
      }
    }

    ////////////////////////

    size_t ist = v._start + 1;
    size_t ien = v._end - 1;

    for (size_t i = ist; i <= ien;) {
      const Token *vt_tok = v.token(i);
      const Token *dt_tok = v.token(i + 1);
      const Token *nam_tok = v.token(i + 2);

      // printf( "  ParseFxInterface Tok<%s>\n", vt_tok->text.c_str() );

      if (vt_tok->text == "layout") {
        std::string layline;
        bool done = false;
        bool is_input = false;
        bool has_punc = false;
        bool is_points = false;
        bool is_lines = false;
        bool is_tris = false;

        while (false == done) {
          const auto &txt = vt_tok->text;

          is_input |= (txt == "in");
          is_points |= (txt == "points");
          is_lines |= (txt == "lines");
          is_tris |= (txt == "triangles");

          bool is_punc = (txt == "(") || (txt == ")") || (txt == ",");
          has_punc |= is_punc;

          // if( is_punc )
          layline += " ";
          layline += vt_tok->text;
          // if( is_punc )
          layline += " ";
          done = vt_tok->text == ";";
          i++;
          vt_tok = v.token(i);
        }
        layline += "\n";
        psi->mPreamble.push_back(layline);

        if (has_punc && is_input) {
          if (is_points)
            psi->mGsPrimSize = 1;
          if (is_lines)
            psi->mGsPrimSize = 2;
          if (is_tris)
            psi->mGsPrimSize = 3;
        }

      } else if (vt_tok->text == "in") {
        auto it = psi->mAttributes.find(nam_tok->text);
        assert(
            it ==
            psi->mAttributes.end()); // make sure there are no duplicate attrs

        int iloc = int(psi->mAttributes.size());
        Attribute *pattr = new Attribute(nam_tok->text);
        pattr->mTypeName = dt_tok->text;
        pattr->mDirection = "in";

        psi->mAttributes[nam_tok->text] = pattr;
        if (v.token(i + 3)->text == ":") {
          pattr->mSemantic = v.token(i + 4)->text;
          // printf( "SEMANTIC<%s>\n", pattr->mSemantic.c_str() );
          i += 6;
        } else if (v.token(i + 3)->text == ";") {
          i += 4;
        } else if (v.token(i + 3)->text == "[") {
          pattr->mArraySize = atoi(v.token(i + 4)->text.c_str());
          i += 7;
        } else {
          assert(false);
        }
        pattr->mLocation = int(psi->mAttributes.size());

      } else if (vt_tok->text == "out") {
        int iloc = int(psi->mAttributes.size());
        Attribute *pattr = new Attribute(nam_tok->text);
        pattr->mTypeName = dt_tok->text;
        pattr->mDirection = "out";
        pattr->mLocation = iloc;
        psi->mAttributes[nam_tok->text] = pattr;

        if (v.token(i + 3)->text == ";") {
          i += 4;
        } else if (v.token(i + 3)->text == "[") {
          pattr->mArraySize = atoi(v.token(i + 4)->text.c_str());
          i += 7;
        } else {
          assert(false);
        }
      } else if (vt_tok->text == "\n") {
        i++;
      } else {
        printf("invalid token<%s>\n", vt_tok->text.c_str());
        OrkAssert(false);
      }
    }

    ////////////////////////
    // sort attributes for performance
    //  (see
    //  http://stackoverflow.com/questions/16415037/opengl-core-profile-incredible-slowdown-on-os-x)
    ////////////////////////

    std::multimap<int, Attribute *> attr_sort_map;
    for (const auto &it : psi->mAttributes) {
      auto attr = it.second;
      auto itloc = gattrsorter.find(attr->mSemantic);
      int isort = 100;
      if (itloc != gattrsorter.end()) {
        isort = itloc->second;
      }
      attr_sort_map.insert(std::make_pair(isort, attr));
      // pattr->mLocation = itloc->second;
    }

    int isort = 0;
    for (const auto &it : attr_sort_map) {
      auto attr = it.second;
      attr->mLocation = isort++;
    }

    ////////////////////////
    itokidx = v.blockEnd() + 1;
    return psi;
  }
  ///////////////////////////////////////////////////////////
  StateBlock *ParseFxStateBlock() {
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
          psb->addStateFn([=](GfxTarget *t) {
            t->RSI()->SetCullTest(lev2::ECULLTEST_OFF);
          });
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
  LibBlock *ParseLibraryBlock() {
    auto pret = new LibBlock(scanner);
    pret->mView->scanBlock(itokidx);
    pret->mName = scanner.tokens[itokidx + 1].text;
    itokidx = pret->mView->blockEnd() + 1;
    return pret;
  }
  ///////////////////////////////////////////////////////////
  int ParseFxShaderCommon(Shader *pshader) {
    bool bkill = false;

    ScanViewRegex r("()", true);
    ScannerView v(scanner, r);
    v.scanBlock(itokidx);
    // v.Dump();

    pshader->mpContainer = mpContainer;

    ///////////////////////////////////
    std::string shadername = v.blockName();

    LibBlock *plibblock = nullptr;

    const Token *ptok = nullptr;
    // int itok = v._blockName+1;

    //////////////////////////////////////////////
    // enumerate lib blocks / interfaces
    //////////////////////////////////////////////

    bool is_vertex_shader = pshader->mShaderType == GL_VERTEX_SHADER;
    bool is_tessctrl_shader = pshader->mShaderType == GL_TESS_CONTROL_SHADER;
    bool is_tesseval_shader = pshader->mShaderType == GL_TESS_EVALUATION_SHADER;
    bool is_geometry_shader = pshader->mShaderType == GL_GEOMETRY_SHADER;
    bool is_fragment_shader = pshader->mShaderType == GL_FRAGMENT_SHADER;

    std::vector<LibBlock *> lib_blocks;

    StreamInterface *iface = nullptr;

    {
      size_t inumdecos = v.numBlockDecorators();

      for (size_t ideco = 0; ideco < inumdecos; ideco++) {
        ptok = v.blockDecorator(ideco);
        int block_deco_index = v._blockDecorators[ideco];

        auto it_lib = mpContainer->_libBlocks.find(ptok->text);
        auto it_vi = mpContainer->_vertexInterfaces.find(ptok->text);
        auto it_tc = mpContainer->_tessCtrlInterfaces.find(ptok->text);
        auto it_te = mpContainer->_tessEvalInterfaces.find(ptok->text);
        auto it_gi = mpContainer->_geometryInterfaces.find(ptok->text);
        auto it_fi = mpContainer->_fragmentInterfaces.find(ptok->text);

        if (it_lib != mpContainer->_libBlocks.end()) {
          auto plibblock = it_lib->second;
          lib_blocks.push_back(plibblock);
          // printf( "LIBBLOCK <%s>\n", ptok->text.c_str() );
        } else if (is_vertex_shader &&
                   it_vi != (mpContainer->_vertexInterfaces.end())) {
          iface = mpContainer->vertexInterface(ptok->text);
          pshader->mpInterface = iface;
          // printf( "VINF <%s>\n", ptok->text.c_str() );
        } else if (is_tessctrl_shader &&
                   (it_tc != mpContainer->_tessCtrlInterfaces.end())) {
          iface = mpContainer->tessCtrlInterface(ptok->text);
          pshader->mpInterface = iface;
          // printf( "TCINF <%s>\n", ptok->text.c_str() );
        } else if (is_tesseval_shader &&
                   (it_te != mpContainer->_tessEvalInterfaces.end())) {
          iface = mpContainer->tessEvalInterface(ptok->text);
          pshader->mpInterface = iface;
          // printf( "TEINF <%s>\n", ptok->text.c_str() );
        } else if (is_geometry_shader &&
                   (it_gi != mpContainer->_geometryInterfaces.end())) {
          iface = mpContainer->geometryInterface(ptok->text);
          pshader->mpInterface = iface;
          // printf( "GINF <%s>\n", ptok->text.c_str() );
        } else if (is_fragment_shader &&
                   (it_fi != mpContainer->_fragmentInterfaces.end())) {
          iface = mpContainer->fragmentInterface(ptok->text);
          pshader->mpInterface = iface;
          // printf( "FINF <%s>\n", ptok->text.c_str() );
        } else if (ptok->text == "extension") {
          auto lparen = scanner.token(block_deco_index + 1);
          auto rparen = scanner.token(block_deco_index + 3);
          assert(lparen->text == "(");
          assert(rparen->text == ")");
          auto extid = scanner.token(block_deco_index + 2);
          pshader->requireExtension(extid->text);
        } else {
          printf("bad shader interface decorator!\n");
          printf("shader<%s>\n", shadername.c_str());
          printf("deco<%s>\n", ptok->text.c_str());
          printf("is_vtx<%d> is_geo<%d> is_frg<%d>\n", int(is_vertex_shader),
                 int(is_geometry_shader), int(is_fragment_shader));
          assert(false);
        }
      }
    }

    //////////////////////////////////////////////

    assert(iface != nullptr);

    //////////////////////////////////////////////
    // printf( "ParseFxShaderCommon Eob<%d> Next<%s>\n", iend, etok.text.c_str()
    // );
    ///////////////////////////////////

    std::string shaderbody;

    size_t iline = 1;
    FixedString<64> fxstr;
    auto prline = [&]() {
      fxstr.format("/*%03d*/", int(iline));
      shaderbody += fxstr.c_str();
      iline++;
    };

    prline();

    shaderbody += "#version 410 core\n";

    ////////////////////////////////////////////////////////////////////////////
    // declare required extensions
    ////////////////////////////////////////////////////////////////////////////

    for (auto extension : pshader->_requiredExtensions) {
      prline();
      shaderbody += FormatString("#extension %s : enable\n", extension.c_str());
    }

    ////////////////////////////////////////////////////////////////////////////

    for (const auto &preamble_line : iface->mPreamble) {
      prline();
      shaderbody += preamble_line;
    }

    ///////////////////////
    // UNIFORMS
    ///////////////////////

    for (const auto &ub : iface->_uniformSets) {
      for (auto itu : ub->_uniforms) {
        prline();
        Uniform *pu = itu.second;
        shaderbody += "uniform ";
        shaderbody += pu->mTypeName + " ";
        shaderbody += pu->mName;

        if (pu->mArraySize) {
          ork::FixedString<32> fxs;
          fxs.format("[%d]", pu->mArraySize);
          shaderbody += std::string(fxs.c_str());
        }

        shaderbody += ";\n";
      }
    }
    ///////////////////////
    // ATTRIBUTES
    ///////////////////////
    for (StreamInterface::AttrMap::const_iterator ita =
             iface->mAttributes.begin();
         ita != iface->mAttributes.end(); ita++) {
      prline();
      Attribute *pa = ita->second;

      shaderbody += pa->mDirection + " ";
      shaderbody += pa->mTypeName + " ";

      if (pa->mArraySize) {
        ork::FixedString<128> fxs;
        // fxs.format("%s[%d]", pa->mName.c_str(), pa->mArraySize );
        fxs.format("%s[]", pa->mName.c_str());
        shaderbody += fxs.c_str();
      } else
        shaderbody += pa->mName;

      shaderbody += ";";

      if (pa->mComment.length()) {
        shaderbody += pa->mComment;
        bkill = true;
      }

      shaderbody += "\n";
    }
    ///////////////////////////////////
    auto code_inject = [&](const ScannerView &view) {
      int ist = view._start + 1;
      int ien = view._end - 1;

      bool bnewline = true;

      size_t column = 0;
      int indent = 1;

      for (size_t i = ist; i <= ien; i++) {
        ptok = view.token(i);

        if (bnewline) {
          prline();

          for (int in = 0; in < indent; in++)
            shaderbody += "\t";
          column = 0;
        }

        const std::string &cur_tok = ptok->text;
        // printf( "  ParseFxShaderCommon Tok<%s>\n", cur_tok.c_str() );
        shaderbody += cur_tok;

        // if( column < 2 )
        shaderbody += " ";

        column++;

        bnewline = false;
        if (cur_tok == "\n") {
          bnewline = true;
        } else if (cur_tok == "{")
          indent++;
        else if (cur_tok == "}")
          indent--;
      }
    };

    ///////////////////////////////////
    // inject libblock code
    ///////////////////////////////////
    for (const auto &libblk : lib_blocks) {
      prline();
      shaderbody += "// libblock<" + libblk->mName +
                    "> ///////////////////////////////////\n";

      const ScannerView &lib_view = *libblk->mView;
      // printf( "LibBlockView.Start<%d> LibBlockView.End<%d>
      // scanner.numtoks<%d> view.numtoks<%d>\n", view._start,view._end,
      // int(scanner.tokens.size()), int(view.mIndices.size()) );
      code_inject(lib_view);

      shaderbody += "//////////////////////////////////////////////////////////"
                    "/////////\n";
    }
    ///////////////////////////////////
    prline();
    shaderbody += "void " + shadername + "()\n{";

    size_t iblockstart = v._start;
    size_t iblockend = v._end;

    code_inject(v);

    shaderbody += "}\n";
    ///////////////////////////////////
    // printf( "shaderbody\n" );
    // printf( "///////////////////////////////\n" );
    // printf( "%s", shaderbody.c_str() );
    // printf( "///////////////////////////////\n" );
    ///////////////////////////////////
    pshader->mName = shadername;
    pshader->mShaderText = shaderbody;
    ///////////////////////////////////
    int new_end = v.blockEnd() + 1;
    // printf( "newend be<%d> deref<%d>\n", int(iblockend), new_end );

    // assert(false==bkill);

    return new_end;
  }
  ///////////////////////////////////////////////////////////
  ShaderVtx *ParseFxVertexShader() {
    auto pshader = new ShaderVtx();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
  ///////////////////////////////////////////////////////////
  ShaderTsC *ParseFxTessCtrlShader() {
    auto pshader = new ShaderTsC();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
  ///////////////////////////////////////////////////////////
  ShaderTsE *ParseFxTessEvalShader() {
    auto pshader = new ShaderTsE();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
  ///////////////////////////////////////////////////////////
  ShaderGeo *ParseFxGeometryShader() {
    auto pshader = new ShaderGeo();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
  ///////////////////////////////////////////////////////////
  ShaderFrg *ParseFxFragmentShader() {
    auto pshader = new ShaderFrg();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
#if defined(ENABLE_NVMESH_SHADERS)
  ShaderNvTask *ParseFxNvTaskShader() {
    auto pshader = new ShaderNvTask();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
  ShaderNvMesh *ParseFxNvMeshShader() {
    auto pshader = new ShaderNvMesh();
    itokidx = ParseFxShaderCommon(pshader);
    return pshader;
  }
#endif
  ///////////////////////////////////////////////////////////
  Technique *ParseFxTechnique() {
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
  int ParseFxPass(int istart, Technique *ptek) {
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
  void DumpAllTokens() {
    size_t itokidx = 0;
    const auto &tokens = scanner.tokens;
    while (itokidx < tokens.size()) {
      const Token &tok = tokens[itokidx++];
      // printf( "tok<%d> <%s>\n", itokidx, tok.text.c_str() );
    }
  }
  ///////////////////////////////////////////////////////////
  Container *Parse(const std::string &fxname) {
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
};

/*std::string::const_iterator start = fx_string.begin();
 std::string::const_iterator end = fx_string.end();
 boost::match_results<std::string::const_iterator> what;
 boost::match_flag_type flags = boost::match_default;

 while( boost::regex_search( start, end, what, re_container, flags ) )
 {
 // what[0] contains the whole string
 // what[5] contains the class name.
 // what[6] contains the template specialisation if any.
 // add class name and position to map:
 //m[std::string(what[5].first, what[5].second)
 //  + std::string(what[6].first, what[6].second)]
 //= what[5].first - file.begin();

 const char* match_start = fx_string.c_str()+(what[1].first-fx_string.begin());
 const char* match_end = fx_string.c_str()+(what[1].second-fx_string.begin());

 printf( "what5<%s> end<%s>\n", what[1].str().c_str(), match_end );
 // update search position:
 start = what[0].second;
 // update flags:
 flags |= boost::match_prev_avail;
 flags |= boost::match_not_bob;

 }
 //	printf( "nummatch<%d>\n", int(res.size()) );

 //	boost::sregex_iterator it(fx_string.begin(), fx_string.end(),
 re_identifier );
 //  boost::sregex_iterator end;

 //for (; it != end; ++it)
 //{
 //  printf( "match<%s>\n", it->str().c_str() );
 // v.push_back(it->str()); or something similar
 //}*/

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
