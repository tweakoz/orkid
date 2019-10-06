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

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

int GlSlFxParser::ParseFxShaderCommon(Shader *pshader) {
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
  // UNIFORM Set
  ///////////////////////

  for (const auto &ub : iface->_uniformSets) {
    for (auto itu : ub->_uniforms) {
      prline();
      shaderbody += "uniform ";
      shaderbody += itu.second->genshaderbody();
      shaderbody += ";\n";
    }
  }

  ///////////////////////
  // UNIFORM Block
  ///////////////////////

  for (const auto &ub : iface->_uniformBlocks) {
    prline();
    shaderbody += FormatString("layout (std140) uniform %s {\n",ub->_name.c_str() );
    for (auto itsub : ub->_subuniforms) {
      prline();
      shaderbody += itsub.second->genshaderbody();
      shaderbody += ";\n";
    }
    prline();
    shaderbody += "};\n";
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
ShaderVtx *GlSlFxParser::ParseFxVertexShader() {
  auto pshader = new ShaderVtx();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderTsC *GlSlFxParser::ParseFxTessCtrlShader() {
  auto pshader = new ShaderTsC();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderTsE *GlSlFxParser::ParseFxTessEvalShader() {
  auto pshader = new ShaderTsE();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderGeo *GlSlFxParser::ParseFxGeometryShader() {
  auto pshader = new ShaderGeo();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
///////////////////////////////////////////////////////////
ShaderFrg *GlSlFxParser::ParseFxFragmentShader() {
  auto pshader = new ShaderFrg();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
#if defined(ENABLE_NVMESH_SHADERS)
ShaderNvTask *GlSlFxParser::ParseFxNvTaskShader() {
  auto pshader = new ShaderNvTask();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
ShaderNvMesh *GlSlFxParser::ParseFxNvMeshShader() {
  auto pshader = new ShaderNvMesh();
  itokidx = ParseFxShaderCommon(pshader);
  return pshader;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
