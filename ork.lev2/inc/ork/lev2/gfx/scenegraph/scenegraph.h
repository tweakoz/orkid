////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2::scenegraph {

struct Layer;
struct Node;
struct DrawableNode;
struct InstancedDrawableNode;
struct LightNode;
struct Scene;
using layer_ptr_t        = std::shared_ptr<Layer>;
using node_ptr_t         = std::shared_ptr<Node>;
using node_atomicptr_t   = std::atomic<node_ptr_t>;
using scene_ptr_t        = std::shared_ptr<Scene>;
using drawable_node_ptr_t = std::shared_ptr<DrawableNode>;
using instanced_drawable_node_ptr_t = std::shared_ptr<InstancedDrawableNode>;
using lightnode_ptr_t    = std::shared_ptr<LightNode>;

///////////////////////////////////////////////////////////////////////////////

struct Node {

  Node(std::string named);
  virtual ~Node();

  std::string _name;
  DrawQueueXfData _dqxfdata;
  varmap::varmap_ptr_t _userdata;
  bool _enabled = true;
};

///////////////////////////////////////////////////////////////////////////////

struct DrawableNode final : public Node {

  DrawableNode(std::string named, drawable_ptr_t drawable);
  ~DrawableNode();

  drawable_ptr_t _drawable;
  fvec4 _modcolor;
};

///////////////////////////////////////////////////////////////////////////////
// InstancedDrawableNode: a scenegraph node
//  that is part of another instancing group
///////////////////////////////////////////////////////////////////////////////

struct InstancedDrawableNode final : public Node {

  InstancedDrawableNode(std::string named, instanced_drawable_ptr_t drawable);
  ~InstancedDrawableNode();

  instanced_drawable_ptr_t _drawable;
  size_t _instanced_drawable_id = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct LightNode final : public Node {

  LightNode(std::string named, light_ptr_t light);
  ~LightNode();

  light_ptr_t _light;
};

///////////////////////////////////////////////////////////////////////////////

struct Layer {

  using drawablenodevect_t = std::vector<drawable_node_ptr_t>;
  using instanced_drawablenodevect_t = std::vector<instanced_drawable_node_ptr_t>;
  using instanced_drawmap_t = std::unordered_map<instanced_drawable_ptr_t,instanced_drawablenodevect_t>;
  using lightnodevect_t = std::vector<lightnode_ptr_t>;

  Layer(Scene* scene, std::string name);
  ~Layer();

  //! create/remove drawable nodes

  drawable_node_ptr_t createDrawableNode(std::string named, drawable_ptr_t drawable);
  void removeDrawableNode(drawable_node_ptr_t node);

  //! create/remove "instanced" drawable node
  /*!
      create an instanced node and assign as an instance in parent_drawable
  */

  instanced_drawable_node_ptr_t createInstancedDrawableNode(std::string named, instanced_drawable_ptr_t parent_drawable);
  void removeInstancedDrawableNode(instanced_drawable_node_ptr_t node);

  //! create/remove drawable nodes

  lightnode_ptr_t createLightNode(std::string named, light_ptr_t drawable);
  void removeLightNode(lightnode_ptr_t node);

  //
  Scene* _scene = nullptr;

  std::string _name;

  LockedResource<drawablenodevect_t> _drawable_nodes;
  LockedResource<instanced_drawmap_t> _instanced_drawable_map;
  LockedResource<lightnodevect_t> _lightnodes;
};

///////////////////////////////////////////////////////////////////////////
struct PickBuffer {
  PickBuffer(ork::lev2::Context* ctx, Scene& scene);
  void mydraw(fray3_constptr_t ray);
  uint64_t pickWithRay(fray3_constptr_t ray);
  uint64_t pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord);
  lev2::Context* _context    = nullptr;
  CompositingData* _compdata = nullptr;

  Scene& _scene;
  lev2::PixelFetchContext _pixelfetchctx;
  compositorimpl_ptr_t _compimpl;
  fmtx4_ptr_t _pick_mvp_matrix;
  CameraData _camdat;
};
using pickbuffer_ptr_t = std::shared_ptr<PickBuffer>;

///////////////////////////////////////////////////////////////////////////////

struct DrawableDataKvPair : public ork::Object {
  DeclareConcreteX(DrawableDataKvPair, ork::Object);
public:
  std::string _layername;
  lev2::drawabledata_ptr_t _drawabledata;
};
struct DrawableKvPair  : public ork::Object {
  std::string _layername;
  lev2::drawable_ptr_t _drawable;
};

using drawabledatakvpair_ptr_t = std::shared_ptr<DrawableDataKvPair>;

struct Scene {

  Scene();
  Scene(varmap::varmap_ptr_t _initialdata);
  ~Scene();


  void initWithParams(varmap::varmap_ptr_t _initialdata);

  layer_ptr_t createLayer(std::string named);
  layer_ptr_t findLayer(std::string named);

  using on_enqueue_fn_t = std::function<void(DrawableBuffer* DB)>;

  void enqueueToRenderer(cameradatalut_ptr_t cameras,on_enqueue_fn_t on_enqueue=[](DrawableBuffer* DB){});
  void renderOnContext(Context* ctx);
  void renderOnContext(Context* ctx,RenderContextFrameData& RCFD);
  void renderWithStandardCompositorFrame(standardcompositorframe_ptr_t sframe);

  void _renderIMPL(Context* ctx,RenderContextFrameData& RCFD);

  void gpuInit(Context* ctx);
  void gpuExit(Context* ctx);

  uint64_t pickWithRay(fray3_constptr_t ray);
  uint64_t pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord);

  template <typename T> T* tryRenderNodeAs() {
    return dynamic_cast<T*>(_renderNode);
  }
  template <typename T> T* tryOutputNodeAs() {
    return dynamic_cast<T*>(_outputNode);
  }

  dbufcontext_ptr_t _dbufcontext_SG;
  irenderer_ptr_t _renderer;
  lightmanager_ptr_t _lightManager;
  lightmanagerdata_ptr_t _lightManagerData;
  compositorimpl_ptr_t _compositorImpl;
  compositordata_ptr_t _compositorData;
  pickbuffer_ptr_t _pickbuffer;
  NodeCompositingTechnique* _compostorTechnique = nullptr;
  OutputCompositingNode* _outputNode            = nullptr;
  RenderCompositingNode* _renderNode = nullptr;
  compositingpassdata_ptr_t _topCPD;
  RenderPresetContext _compositorPreset;
  std::vector<DrawableKvPair> _staticDrawables; //! global drawables owned by the scenegraph, not owned by nodes...

  using layer_map_t = std::map<std::string, layer_ptr_t>;

  LockedResource<layer_map_t> _layers;
  varmap::varmap_ptr_t _userdata;
  bool _dogpuinit        = true;
  Context* _boundContext = nullptr;

  struct DrawItem{
    ork::lev2::DrawableBufLayer * _layer;
    drawable_node_ptr_t _drwnode;
  };

  std::vector<DrawItem> _nodes2draw;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
