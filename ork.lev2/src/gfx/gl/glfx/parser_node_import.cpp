////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/kernel/string/string.h>
#include <ork/kernel/string/deco.inl>
#include <regex>
#include <stdlib.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ImportNode::pregen(shaderbuilder::BackEnd& backend) const {



}

/////////////////////////////////////////////////////////////////////////////////////////////////

void ImportNode::load(){
    file::Path::NameType a, b;
    file::Path imppath;
    auto parent_parser = _parent_topnode->_parser;
    //////////////////////////////////
    // if import file has datasource (xxx://), use that
    //  instead of inferring from container's path
    //////////////////////////////////
    file::Path(_name).split(a, b, ':');
    if (b.length() != 0) { // use from import
      imppath = _name;
    } else { // infer from container
      imppath = parent_parser->_name.c_str();
      //printf( "parent_parser<%s> imppath1<%s>\n", parent_parser->_name.c_str(), imppath.c_str());
      imppath.split(a, b, ':');
      ork::FixedString<256> fxs;
      fxs.format("%s://%s", a.c_str(), _name.c_str());
      imppath = fxs.c_str();
    }
    auto program = parent_parser->_program;
    //printf( "progam<%p:%s> imppath<%s>\n", program.get(), program->_name.c_str(), imppath.c_str());
    auto importscanner = std::make_shared<Scanner>(block_regex);
    ///////////////////////////////////
    File fx_file(imppath.c_str(), EFM_READ);
    ///////////////////////////////////
    OrkAssert(fx_file.IsOpen());
    EFileErrCode eFileErr = fx_file.GetLength(importscanner->ifilelen);
    importscanner->resize(importscanner->ifilelen + 1);
    eFileErr                                          = fx_file.Read(importscanner->_fxbuffer.data(), importscanner->ifilelen);
    importscanner->_fxbuffer[importscanner->ifilelen] = 0;
    performScan(importscanner);
    _parser = std::make_shared<GlSlFxParser>(imppath.c_str(),program,importscanner);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx::parser {
/////////////////////////////////////////////////////////////////////////////////////////////////
