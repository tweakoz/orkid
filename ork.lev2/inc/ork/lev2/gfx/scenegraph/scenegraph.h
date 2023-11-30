////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
struct CameraNode;
struct InstancedDrawableNode;
struct LightNode;
struct Scene;
using layer_ptr_t        = std::shared_ptr<Layer>;
using node_ptr_t         = std::shared_ptr<Node>;
using node_atomicptr_t   = std::atomic<node_ptr_t>;
using scene_ptr_t        = std::shared_ptr<Scene>;
using drawable_node_ptr_t = std::shared_ptr<DrawableNode>;
using camera_node_ptr_t = std::shared_ptr<CameraNode>;
using instanced_drawable_node_ptr_t = std::shared_ptr<InstancedDrawableNode>;
using lightnode_ptr_t    = std::shared_ptr<LightNode>;

///////////////////////////////////////////////////////////////////////////////

struct Node : public ork::Object {

  DeclareAbstractX(Node, ork::Object);
public:

  Node(std::string named);
  //virtual ~Node();

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

struct CameraeNode final : public Node {

  CameraeNode(std::string named, cameradata_ptr_t cameradata);
  ~CameraeNode();

  cameradata_ptr_t _drawable;

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

  template <typename T, typename... A> //
  node_ptr_t makeDrawableNodeWithPrimitive(std::string named, A&&... prim_args) { //
    auto drawable = T::makeDrawableAndPrimitive(std::forward<A>(prim_args)...); 
    return this->createDrawableNode(named, drawable);
  }

  //
  Scene* _scene = nullptr;

  std::string _name;

  LockedResource<drawablenodevect_t> _drawable_nodes;
  LockedResource<instanced_drawmap_t> _instanced_drawable_map;
  LockedResource<lightnodevect_t> _lightnodes;

  uint32_t _sortkey = 0;
};

///////////////////////////////////////////////////////////////////////////
struct SgPickBuffer {

  using callback_t = std::function<void(pickvariant_t)>;

  SgPickBuffer(ork::lev2::Context* ctx, Scene& scene);
  void mydraw(fray3_constptr_t ray);
  void pickWithRay(fray3_constptr_t ray, callback_t callback);
  void pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, callback_t callback);
  lev2::Context* _context    = nullptr;
  CompositingData* _compdata = nullptr;

  Scene& _scene;
  lev2::pixelfetchctx_ptr_t _pfc;
  compositorimpl_ptr_t _compimpl;
  fmtx4_ptr_t _pick_mvp_matrix;
  CameraData _camdat;
  const ork::lev2::Texture* _picktexture = nullptr;
};
using sgpickbuffer_ptr_t = std::shared_ptr<SgPickBuffer>;

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

struct Synchro {
  Synchro();
  ~Synchro();
  void terminate();
  bool beginUpdate();
  void endUpdate();
  bool beginRender();
  void endRender();
  std::atomic<int> _updcount;
  std::atomic<int> _rencount;
};

using synchro_ptr_t = std::shared_ptr<Synchro>;

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
  void renderOnContext(Context* ctx,rcfd_ptr_t RCFD);
  void renderWithStandardCompositorFrame(standardcompositorframe_ptr_t sframe);

  void _renderIMPL(Context* ctx,rcfd_ptr_t RCFD);
  void _renderWithAcquiredRenderDrawBuffer(acqdrawbuffer_constptr_t acqbuf);

  void gpuInit(Context* ctx);
  void gpuExit(Context* ctx);

  void pickWithRay(fray3_constptr_t ray, SgPickBuffer::callback_t callback);
  void pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, SgPickBuffer::callback_t callback);

  template <typename T> std::shared_ptr<T> tryRenderNodeAs() {
    return std::dynamic_pointer_cast<T>(_renderNode);
  }
  template <typename T> std::shared_ptr<T> tryOutputNodeAs() {
    return std::dynamic_pointer_cast<T>(_outputNode);
  }

  void enablePickHud();

  dbufcontext_ptr_t _dbufcontext_SG;
  irenderer_ptr_t _renderer;
  lightmanager_ptr_t _lightManager;
  lightmanagerdata_ptr_t _lightManagerData;
  compositorimpl_ptr_t _compositorImpl;
  compositordata_ptr_t _compositorData;
  sgpickbuffer_ptr_t _sgpickbuffer;
  nodecompositortechnique_ptr_t _compositorTechnique = nullptr;
  compositoroutnode_ptr_t _outputNode            = nullptr;
  compositorrendernode_ptr_t _renderNode = nullptr;
  compositingpassdata_ptr_t _topCPD;
  RenderPresetContext _compositorPreset;
  std::vector<DrawableKvPair> _staticDrawables; //! global drawables owned by the scenegraph, not owned by nodes...
  Timer _profile_timer;
  gfxcontext_lambda_t _on_render_complete;
  synchro_ptr_t _synchro;
  float _currentTime = 0.0f;
  
  using layer_map_t = std::map<std::string, layer_ptr_t>;

  LockedResource<layer_map_t> _layers;
  varmap::varmap_ptr_t _userdata;
  varmap::varmap_ptr_t _params;
  bool _dogpuinit        = true;
  Context* _boundContext = nullptr;

  struct DrawItem{
    ork::lev2::DrawableBufLayer * _layer;
    drawable_node_ptr_t _drwnode;
  };

  std::vector<DrawItem> _nodes2draw;
  bool _enable_pick_hud = false;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
