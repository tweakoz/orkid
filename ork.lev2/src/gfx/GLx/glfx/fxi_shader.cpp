////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
bool Shader::IsCompiled() const { return mbCompiled; }
void Shader::setInputInterface(StreamInterface* iface) {
  _inputInterface = iface;
  for (auto uset : iface->_uniformSets)
    addUniformSet(uset);
  for (auto ublk : iface->_uniformBlocks)
    addUniformBlock(ublk);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void Shader::addUniformSet(UniformSet* uset) {
  _unisets.push_back(uset);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void Shader::addUniformBlock(UniformBlock* ublk) {
  _uniblocks.push_back(ublk);
}
////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////