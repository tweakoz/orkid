////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::scenegraph {
///////////////////////////////////////////////////////////////////////////////

struct Layer;
struct Node;
struct DrawableNode;
struct CameraNode;
struct LightNode;
struct ProbeNode;
struct Scene;
struct Synchro;
struct SgPickBuffer;
struct DrawableDataKvPair;

using layer_ptr_t        = std::shared_ptr<Layer>;
using node_ptr_t         = std::shared_ptr<Node>;
using node_atomicptr_t   = std::atomic<node_ptr_t>;
using scene_ptr_t        = std::shared_ptr<Scene>;
using drawable_node_ptr_t = std::shared_ptr<DrawableNode>;
using camera_node_ptr_t = std::shared_ptr<CameraNode>;
using lightnode_ptr_t    = std::shared_ptr<LightNode>;
using drawabledatakvpair_ptr_t = std::shared_ptr<DrawableDataKvPair>;
using synchro_ptr_t = std::shared_ptr<Synchro>;
using sgpickbuffer_ptr_t = std::shared_ptr<SgPickBuffer>;
using probenode_ptr_t = std::shared_ptr<ProbeNode>;

///////////////////////////////////////////////////////////////////////////////

struct Node : public ork::Object {

  DeclareAbstractX(Node, ork::Object);
public:

  Node(std::string named);
  //virtual ~Node();

  std::string _name;
  DrawQueueTransferData _dqxfdata;
  varmap::varmap_ptr_t _userdata;
  bool _enabled = true;
  bool _pickable = true;
  bool _viewRelative = false;
  std::unordered_set<Layer*> _layers;
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

struct LightNode final : public Node {

  LightNode(std::string named, light_ptr_t light);
  ~LightNode();

  light_ptr_t _light;
};

///////////////////////////////////////////////////////////////////////////////

struct ProbeNode final : public Node {

  ProbeNode(std::string named, lightprobe_ptr_t probe);
  ~ProbeNode();

  lightprobe_ptr_t _probe;
};

///////////////////////////////////////////////////////////////////////////////

struct Layer {

  using drawablenodevect_t = std::vector<drawable_node_ptr_t>;
  using lightnodevect_t = std::vector<lightnode_ptr_t>;
  using probenodevect_t = std::vector<probenode_ptr_t>;

  Layer(Scene* scene, std::string name);
  ~Layer();

  //! create/remove drawable nodes

  drawable_node_ptr_t createDrawableNode(std::string named, drawable_ptr_t drawable);
  void removeDrawableNode(drawable_node_ptr_t node);

  void addDrawableNode(drawable_node_ptr_t node);

  //! create/remove drawable nodes

  lightnode_ptr_t createLightNode(std::string named, light_ptr_t drawable);
  void removeLightNode(lightnode_ptr_t node);

  probenode_ptr_t createProbeNode(std::string named, lightprobe_ptr_t drawable);
  void removeProbeNode(probenode_ptr_t node);

  template <typename T, typename... A> //
  node_ptr_t makeDrawableNodeWithPrimitive(std::string named, A&&... prim_args) { //
    auto drawable = T::makeDrawableAndPrimitive(std::forward<A>(prim_args)...); 
    return this->createDrawableNode(named, drawable);
  }

  //
  Scene* _scene = nullptr;

  std::string _name;

  LockedResource<drawablenodevect_t> _drawable_nodes;
  LockedResource<lightnodevect_t> _lightnodes;
  LockedResource<probenodevect_t> _probenodes;

  uint32_t _sortkey = 0;
};

struct NodeInstanceData  {
  std::string _groupname;
};
struct NodeInstance {

  std::string _groupname;
  instanced_drawable_ptr_t _idrawable;
  instanceddrawinstancedata_ptr_t _idata;
  int _instance_index = -1;
};

using node_instance_data_ptr_t = std::shared_ptr<NodeInstanceData>;
using node_instance_ptr_t = std::shared_ptr<NodeInstance>;

///////////////////////////////////////////////////////////////////////////
struct SgPickBuffer {

  using callback_t = std::function<void(pixelfetchctx_ptr_t)>;

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
  const ork::lev2::Texture* _pickIDtexture = nullptr;
  const ork::lev2::Texture* _pickPOStexture = nullptr;
  const ork::lev2::Texture* _pickNRMtexture = nullptr;
  const ork::lev2::Texture* _pickUVtexture = nullptr;
};

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

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////

struct Scene {

  Scene();
  Scene(varmap::varmap_ptr_t _initialdata);
  ~Scene();
  
  void __common_init();


  void initWithParams(varmap::varmap_ptr_t _initialdata);

  layer_ptr_t createLayer(std::string named);
  layer_ptr_t findLayer(std::string named);

  using on_enqueue_fn_t = std::function<void(DrawQueue* DB)>;

  void enqueueToRenderer(cameradatalut_ptr_t cameras,on_enqueue_fn_t on_enqueue=[](DrawQueue* DB){});
  void renderOnContext(Context* ctx);
  void renderOnContext(Context* ctx,rcfd_ptr_t RCFD);
  void renderWithStandardCompositorFrame(standardcompositorframe_ptr_t sframe);

  void _renderIMPL(Context* ctx,rcfd_ptr_t RCFD);
  void _renderWithAcquiredDrawQueueForRendering(acqdrawbuffer_constptr_t acqbuf);

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
  compositorpostnode_ptr_t getPostNode( size_t index ) const;
  size_t getPostNodeCount() const;

  void enablePickHud();


  render_preset_data_ptr_t _renderPresetData;
  pbr::commonstuff_ptr_t _pbr_common;
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
  uint32_t _pickFormat = 0;
  bool _doResizeFromMainSurface = false;
  using layer_map_t = std::map<std::string, layer_ptr_t>;

  LockedResource<layer_map_t> _layers;
  varmap::varmap_ptr_t _userdata;
  varmap::varmap_ptr_t _params;
  bool _dogpuinit        = true;
  Context* _boundContext = nullptr;

  asset::loadsynchro_ptr_t _loadSynchro;
  bool okToRender() const;
  struct DrawItem{
    ork::lev2::DrawQueueLayer * _layer;
    drawable_node_ptr_t _drwnode;
  };

  std::vector<DrawItem> _nodes2draw;
  bool _enable_pick_hud = false;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
