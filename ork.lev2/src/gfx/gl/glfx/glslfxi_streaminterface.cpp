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
/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
