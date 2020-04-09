////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <unordered_set>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/material_freestyle.inl>

namespace ork::lev2::scenegraph {

struct Layer;
struct Node;
struct SceneGraph;
using layer_ptr_t = std::shared_ptr<Layer>;
using node_ptr_t  = std::shared_ptr<Node>;
using graph_ptr_t = std::shared_ptr<SceneGraph>;

///////////////////////////////////////////////////////////////////////////////

struct Node {

  Node(std::string named, drawable_ptr_t drawable);
  ~Node();

  std::string _name;
  drawable_ptr_t _drawable;
  DrawQueueXfData _transform;
};

///////////////////////////////////////////////////////////////////////////////

struct Layer {
  Layer(std::string name);
  ~Layer();

  node_ptr_t createNode(std::string named, drawable_ptr_t drawable);
  void removeNode(node_ptr_t node);

  std::string _name;
  std::map<std::string, node_ptr_t> _nodemap;
  std::vector<node_ptr_t> _nodevect;
};

///////////////////////////////////////////////////////////////////////////////

struct SceneGraph {

  SceneGraph();
  ~SceneGraph();

  layer_ptr_t createLayer(std::string named);
  void enqueueToRenderer();
  void renderOnContext(Context* ctx);

  DefaultRenderer _renderer;
  lightmanager_ptr_t _lightManager;
  lightmanagerdata_ptr_t _lightData;
  compositorimpl_ptr_t _compositorImpl;
  compositordata_ptr_t _compositorData;
  cameradata_ptr_t _camera;
  cameradatalut_ptr_t _cameras;
  NodeCompositingTechnique* _compostorTechnique = nullptr;
  ScreenOutputCompositingNode* _outputNode      = nullptr;
  lev2::CompositingPassData _topCPD;

  std::map<std::string, layer_ptr_t> _layers;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
