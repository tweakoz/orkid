////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "../gl.h"
#include "glslfxi.h"
#include "glslfxi_parser.h"
#include <ork/kernel/string/string.h>

namespace ork::lev2::glslfx::shaderbuilder {

BackEnd::BackEnd(const ContainerNode* cnode, Container* c) {
  _cnode     = cnode;
  _container = c;
}

bool BackEnd::generate() {

  bool ok = true;

  std::map<int, ContainerNode::nodevect_t> orderednodemap;

  int order = 0;
  ContainerNode::nodevect_t libnodes;
  ContainerNode::nodevect_t shdnodes;
  ContainerNode::nodevect_t sbknodes;
  ContainerNode::nodevect_t teknodes;
  ContainerNode::nodevect_t ifnodes;
  ContainerNode::nodevect_t shnodes;
  ifnodes.reserve(128);
  shnodes.reserve(128);

  _cnode->collectNodesOfType<LibraryBlockNode>(libnodes);
  _cnode->collectNodesOfType<ShaderDataNode>(shdnodes);
  _cnode->collectNodesOfType<StateBlockNode>(sbknodes);
  _cnode->collectNodesOfType<TechniqueNode>(teknodes);

  _cnode->collectNodesOfType<VertexInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<TessCtrlInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<TessEvalInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<GeometryInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<FragmentInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<VertexShaderNode>(shnodes);
  _cnode->collectNodesOfType<TessCtrlShaderNode>(shnodes);
  _cnode->collectNodesOfType<TessEvalShaderNode>(shnodes);
  _cnode->collectNodesOfType<GeometryShaderNode>(shnodes);
  _cnode->collectNodesOfType<FragmentShaderNode>(shnodes);

  #if defined(ENABLE_COMPUTE_SHADERS)
  _cnode->collectNodesOfType<ComputeInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<ComputeShaderNode>(shnodes);
  #endif
  #if defined(ENABLE_NVMESH_SHADERS)
  _cnode->collectNodesOfType<NvTaskInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<NvMeshInterfaceNode>(ifnodes);
  _cnode->collectNodesOfType<NvTaskShaderNode>(shnodes);
  _cnode->collectNodesOfType<NvMeshShaderNode>(shnodes);
  #endif

  /////////////////////////////////////////////////////
  // generate library blocks, shaderdata and stateblock nodes
  /////////////////////////////////////////////////////

  for( auto item : libnodes ) item->generate(*this);
  for( auto item : shdnodes ) item->generate(*this);
  for( auto item : sbknodes ) item->generate(*this);

  /////////////////////////////////////////////////////
  // generate interface and shader
  /////////////////////////////////////////////////////

  for( auto item : ifnodes ) item->generate(*this);
  for( auto item : shnodes ) item->generate(*this);

  /////////////////////////////////////////////////////
  // generate technique nodes
  /////////////////////////////////////////////////////

  for( auto item : teknodes ) item->generate(*this);

  /////////////////////////////////////////////////////

  return ok;
}

}