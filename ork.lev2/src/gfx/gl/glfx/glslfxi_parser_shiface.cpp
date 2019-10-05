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
StreamInterface *GlSlFxParser::ParseFxInterface(GLenum iftype) {
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

      auto par =
          is_geo ? it_fig->second
                 : is_vtx ? it_fiv->second : is_tee ? it_fie->second : nullptr;

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
      assert(it ==
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
/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
