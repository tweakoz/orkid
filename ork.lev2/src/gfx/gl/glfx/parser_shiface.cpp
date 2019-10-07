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

///////////////////////////////////////////////////////////
StreamInterface* GlSlFxParser::ParseFxInterface(GLenum iftype) {
  ScanViewRegex r("(\n)", true);
  ScannerView v(scanner, r);
  v.scanBlock(itokidx);

  ////////////////////////

  StreamInterface* psi = new StreamInterface;
  psi->mName           = v.blockName();
  psi->mInterfaceType  = iftype;

  ////////////////////////

  const std::string BlockType = v.token(v._blockType)->text;
  bool is_vtx                 = BlockType == "vertex_interface";
  bool is_geo                 = BlockType == "geometry_interface";

  /////////////////////////////
  // interface inheritance
  /////////////////////////////

  size_t inumdecos = v.numBlockDecorators();

  for (size_t ideco = 0; ideco < inumdecos; ideco++) {
    auto ptok = v.blockDecorator(ideco);

    auto it_uniformset = mpContainer->_uniformSets.find(ptok->text);
    auto it_uniformblk = mpContainer->_uniformBlocks.find(ptok->text);

    if (it_uniformset != mpContainer->_uniformSets.end()) {
      psi->_uniformSets.push_back(it_uniformset->second);
    } else if (it_uniformblk != mpContainer->_uniformBlocks.end()) {
      psi->_uniformBlocks.push_back(it_uniformblk->second);
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

      auto par = is_geo ? it_fig->second : is_vtx ? it_fiv->second : is_tee ? it_fie->second : nullptr;

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
  } // for (size_t ideco = 0; ideco < inumdecos; ideco++) {

  ////////////////////////

  size_t ist = v._start + 1;
  size_t ien = v._end - 1;


  std::set<std::string> output_decorators;

  for (size_t i = ist; i <= ien;) {
    const Token* prv_tok  = (i>0) ? v.token(i-1) : nullptr;
    const Token* vt_tok  = v.token(i);
    const Token* dt_tok  = v.token(i + 1);
    const Token* nam_tok = v.token(i + 2);

     printf( "  ParseFxInterface VtTok<%s>\n", vt_tok->text.c_str() );
     printf( "  ParseFxInterface DtTok<%s>\n", dt_tok->text.c_str() );
     printf( "  ParseFxInterface NamTok<%s>\n", nam_tok->text.c_str() );

    ////////////////////////////////////////////////////////////////////////////
    if (vt_tok->text == "layout") {
      ////////////////////////////////////////////////////////////////////////////

      std::string layline;
      bool done      = false;
      bool is_input  = false;
      bool has_punc  = false;
      bool is_points = false;
      bool is_lines  = false;
      bool is_tris   = false;

      while (false == done) {
        const auto& txt = vt_tok->text;

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

      ////////////////////////////////////////////////////////////////////////////
      if (has_punc && is_input) {
        ////////////////////////////////////////////////////////////////////////////
        if (is_points)
          psi->mGsPrimSize = 1;
        if (is_lines)
          psi->mGsPrimSize = 2;
        if (is_tris)
          psi->mGsPrimSize = 3;
      }
    }
    ////////////////////////////////////////////////////////////////////////////
    else if (vt_tok->text == "in") {
      ////////////////////////////////////////////////////////////////////////////
      auto it = psi->mAttributes.find(nam_tok->text);
      assert(it == psi->mAttributes.end()); // make sure there are no duplicate attrs

      int iloc          = int(psi->mAttributes.size());
      Attribute* pattr  = new Attribute(nam_tok->text);
      pattr->mTypeName  = dt_tok->text;
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
      ////////////////////////////////////////////////////////////////////////////
      else if (vt_tok->text == "perprimitiveNV") {
        output_decorators.insert("perprimitiveNV");
      }
    ////////////////////////////////////////////////////////////////////////////
    } else if (vt_tok->text == "out") {
      int iloc                        = int(psi->mAttributes.size());
      Attribute* pattr                = new Attribute(nam_tok->text);
      pattr->mTypeName                = dt_tok->text;
      pattr->mDirection               = "out";
      pattr->mLocation                = iloc;
      psi->mAttributes[nam_tok->text] = pattr;

      ///////////////////////////////////////////

      auto parse_array = [&](int j) -> int {
        if (v.token(j)->text == "[") {
          if (v.token(j + 1)->text == "]") {
            // unsized array
            pattr->mArraySize = -1;
            j += 2;
          } else {
            assert(v.token(j + 3)->text == "]");
            pattr->mArraySize = atoi(v.token(j + 2)->text.c_str());
            j += 3;
          }
        }
        return j;
      };

      ///////////////////////////////////////////
      // inline struct type ?
      ///////////////////////////////////////////

      const Token* structend_tok = nullptr;
      if (v.token(i + 2)->text == "{") {
        pattr->_typeIsInlineStruct = true;
        // struct output
        int closer = -1;
        for (int s = 1; (s < 100) and (closer == -1); s++) {
          const Token* test_tok = v.token(i + s);
          if (test_tok->text == "}"){
            closer = s;
          }
          pattr->_inlineStructToks.push_back(test_tok->text);
        }
        assert(closer > 0);
        int j = parse_array(i+closer+2);
        if( j>(i+closer+2)){ // array of inline struct
          i += closer +5; // i now points
        }
        else {
          i += closer +3; // i now points
        }
        structend_tok = v.token(i);
      }

      ///////////////////////////////////////////

      if (v.token(i + 3)->text == ";") {
        i += 4;
      } else if (v.token(i + 3)->text == "[") {
        i = parse_array(i + 3);
        assert(i > 0);
      }
      output_decorators.clear();
      ////////////////////////////////////////////////////////////////////////////
    } else if (vt_tok->text == "\n" or vt_tok->text == ";") {
      i++;
    } else {
      ////////////////////////////////////////////////////////////////////////////
      printf("invalid token<%s>\n", vt_tok->text.c_str());
      OrkAssert(false);
    }
  }
  ////////////////////////
  // sort attributes for performance
  //  (see
  //  http://stackoverflow.com/questions/16415037/opengl-core-profile-incredible-slowdown-on-os-x)
  ////////////////////////

  std::multimap<int, Attribute*> attr_sort_map;
  for (const auto& it : psi->mAttributes) {
    auto attr  = it.second;
    auto itloc = gattrsorter.find(attr->mSemantic);
    int isort  = 100;
    if (itloc != gattrsorter.end()) {
      isort = itloc->second;
    }
    attr_sort_map.insert(std::make_pair(isort, attr));
    // pattr->mLocation = itloc->second;
  }

  int isort = 0;
  for (const auto& it : attr_sort_map) {
    auto attr       = it.second;
    attr->mLocation = isort++;
  }

  ////////////////////////
  itokidx = v.blockEnd() + 1;
  return psi;
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
