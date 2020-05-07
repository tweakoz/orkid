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
#include <ork/math/line.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2::scenegraph {

struct Layer;
struct Node;
struct DrawableNode;
struct LightNode;
struct Scene;
using layer_ptr_t        = std::shared_ptr<Layer>;
using node_ptr_t         = std::shared_ptr<Node>;
using scene_ptr_t        = std::shared_ptr<Scene>;
using drawablenode_ptr_t = std::shared_ptr<DrawableNode>;
using lightnode_ptr_t    = std::shared_ptr<LightNode>;

///////////////////////////////////////////////////////////////////////////////

struct Node {

  Node(std::string named);
  virtual ~Node();

  std::string _name;
  DrawQueueXfData _transform;
  varmap::varmap_ptr_t _userdata;
};

///////////////////////////////////////////////////////////////////////////////

struct DrawableNode final : public Node {

  DrawableNode(std::string named, drawable_ptr_t drawable);
  ~DrawableNode();

  drawable_ptr_t _drawable;
};

///////////////////////////////////////////////////////////////////////////////

struct LightNode final : public Node {

  LightNode(std::string named, light_ptr_t light);
  ~LightNode();

  light_ptr_t _light;
};

///////////////////////////////////////////////////////////////////////////////

struct Layer {
  Layer(Scene* scene, std::string name);
  ~Layer();

  drawablenode_ptr_t createDrawableNode(std::string named, drawable_ptr_t drawable);
  void removeDrawableNode(drawablenode_ptr_t node);

  lightnode_ptr_t createLightNode(std::string named, light_ptr_t drawable);
  void removeLightNode(lightnode_ptr_t node);

  std::string _name;
  Scene* _scene = nullptr;
  std::map<std::string, drawablenode_ptr_t> _drawablenode_map;
  std::vector<drawablenode_ptr_t> _drawablenodes;
  std::map<std::string, lightnode_ptr_t> _lightnode_map;
  std::vector<lightnode_ptr_t> _lightnodes;
};

///////////////////////////////////////////////////////////////////////////
struct PickBuffer : public ork::lev2::PickBuffer {
  PickBuffer(ork::lev2::Context* ctx, Scene& scene);
  void Draw(lev2::PixelFetchContext& ctx) final;
  uint64_t pickWithRay(fray3_constptr_t ray);
  Scene& _scene;
  CompositingData* _compdata = nullptr;
  compositorimpl_ptr_t _compimpl;
  fmtx4_ptr_t _pick_mvp_matrix;
};
using pickbuffer_ptr_t = std::shared_ptr<PickBuffer>;

///////////////////////////////////////////////////////////////////////////////

struct Scene {

  Scene();
  Scene(varmap::varmap_ptr_t _initialdata);
  ~Scene();

  void initWithParams(varmap::varmap_ptr_t _initialdata);

  layer_ptr_t createLayer(std::string named);
  void enqueueToRenderer(cameradatalut_ptr_t cameras);
  void renderOnContext(Context* ctx);
  void gpuInit(Context* ctx);
  uint64_t pickWithRay(fray3_constptr_t ray);
  DefaultRenderer _renderer;
  lightmanager_ptr_t _lightManager;
  lightmanagerdata_ptr_t _lightManagerData;
  compositorimpl_ptr_t _compositorImpl;
  compositordata_ptr_t _compositorData;
  pickbuffer_ptr_t _pickbuffer;
  NodeCompositingTechnique* _compostorTechnique = nullptr;
  OutputCompositingNode* _outputNode            = nullptr;
  lev2::CompositingPassData _topCPD;

  std::map<std::string, layer_ptr_t> _layers;
  varmap::varmap_ptr_t _userdata;
  bool _dogpuinit        = true;
  Context* _boundContext = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
