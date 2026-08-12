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
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/material_freestyle.h>

///////////////////////////////////////////////////////////////////////////////
// HZB occlusion builder lives in ork::lev2; fwd-declared here so Scene can hold one (shared_ptr,
// type-erased deleter) without pulling the full hzb.h into this already-heavy header.
namespace ork::lev2 {
struct HZBBuilder;
using hzbbuilder_ptr_t = std::shared_ptr<HZBBuilder>;
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::scenegraph {
///////////////////////////////////////////////////////////////////////////////

extern const int PICKBUFFER_DIM;

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
  bool _view_relative = false;
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
  void gpuInit(ork::lev2::Context* ctx);  // Initialize RTG and textures for pick HUD visibility
  void mydraw(fray3_constptr_t ray, callback_t callback);
  void pickWithRay(fray3_constptr_t ray, callback_t callback);
  void pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, const ViewportRect& vprect, callback_t callback);
  lev2::Context* _context    = nullptr;
  CompositingData* _compdata = nullptr;

  Scene& _scene;
  lev2::pixelfetchctx_ptr_t _pfc;
  compositorimpl_ptr_t _compimpl;
  fmtx4_ptr_t _pick_mvp_matrix;
  CameraData _camdat;
  texture_ptr_t _pickIDtexture;
  texture_ptr_t _pickPOStexture;
  texture_ptr_t _pickNRMtexture;
  texture_ptr_t _pickUVtexture;

  // Async pick state
  lev2::captureasync_ptr_t _pendingCapture;
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
  void applyRuntimeParams(varmap::varmap_ptr_t params);

  layer_ptr_t createLayer(std::string named); // create or return existing layer (idempotent)
  layer_ptr_t findLayer(std::string named);   // strict: asserts if layer not found
  layer_ptr_t findLayerMaybe(std::string named); // returns null on miss (no assert)
  layer_ptr_t tryFindLayer(std::string named); // nullable: returns nullptr if layer not found

  //////////////////////////////////////////////////////////////////
  // Layer-role lookup.
  //
  // Compositors ask the scene "which layers render in the depth_prepass
  // role this frame?", rather than hardcoding a single canonical layer
  // name. By default every role resolves to a singleton list containing
  // its own name — so with no overrides, layersForRole("depth_prepass")
  // returns {"depth_prepass"} and the compositor sees identical behavior
  // to the pre-role-lookup code path.
  //
  // Callers (e.g. a multi-ECS scene host) can override a role via
  // setLayerRole() to redirect it to one or more custom layer names.
  // This enables layer-swap-based scene isolation without changing
  // existing caller code.
  //////////////////////////////////////////////////////////////////

  const std::vector<std::string>& layersForRole(const std::string& role) const;
  void setLayerRole(const std::string& role, std::vector<std::string> layers);
  void clearLayerRole(const std::string& role);

  using on_enqueue_fn_t = std::function<void(DrawQueue* DB)>;

  void enqueueToRenderer(cameradatalut_ptr_t cameras,on_enqueue_fn_t on_enqueue=[](DrawQueue* DB){});
  void renderOnContext(Context* ctx);
  void renderOnContext(Context* ctx,rcfd_ptr_t RCFD);
  void renderWithStandardCompositorFrame(standardcompositorframe_ptr_t sframe);

  void _renderIMPL(Context* ctx,rcfd_ptr_t RCFD);
  void _renderWithAcquiredDrawQueueForRendering(acqdrawbuffer_constptr_t acqbuf);

  void gpuInit(Context* ctx);
  // per-FRAME drawable hook fan-out (Drawable::onGpuUpdate). Idempotent per render frame (guarded
  // on the context's frame counter), so every render entry can call it defensively: SGVP
  // gpuUpdateAll, the ECS SceneGraphSystem, AND the direct _renderIMPL paths — first caller wins,
  // a second same-frame call is a no-op (e.g. two viewports sharing one scene).
  void gpuUpdate(Context* ctx);
  // FRAME-PROLOGUE hooks (C++-only): registered callables run exactly once per
  // composited frame (target-frame deduped, render thread), immediately before
  // the compositor assembles — the frame-scoped attach point for system-level
  // GPU work (sky time-of-day push, probe scheduling policy). Every render
  // entry calls the invoker defensively; first caller per frame wins.
  using frame_prologue_hook_t = std::function<void(Context* ctx)>;
  void onFramePrologue(frame_prologue_hook_t hook);
  void _invokeFramePrologueHooks(Context* ctx);
  // per-VIEWPORT pre-render fan-out: invoked from SceneGraphViewport::DoRePaintSurface with that
  // viewport's CameraMatrices; walks enabled drawable nodes and calls Drawable::onPreRender.
  void preRender(Context* ctx, const CameraMatrices& cammtx);
  // per-FRAME sun-shadow cull fan-out (cascade-cull fix): invoked ONCE from the forward prologue with a
  // UNION sun camera enclosing all cascade slices. Walks enabled drawable nodes and calls
  // Drawable::onShadowPreRender on each that wantsShadowCull(), batched into one dispatch phase. When
  // NO drawable wants it the phase is never opened (sunless/non-culled scenes stay byte-identical).
  // ...and CULLSETS narrow it: family_mask restricts the fan-out to the caster
  // families of ONE cullset, so a set is culled exactly once against its own
  // volume. The all-families default is the pre-cullset call verbatim.
  void shadowCull(Context* ctx, const CameraMatrices& cammtx, uint32_t family_mask = kShadowFamilyAll);
  void gpuExit(Context* ctx);

  void pickWithRay(fray3_constptr_t ray, SgPickBuffer::callback_t callback);
  void pickWithScreenCoord(cameradata_ptr_t cam, fvec2 screencoord, const ViewportRect& vprect, SgPickBuffer::callback_t callback);

  template <typename T> std::shared_ptr<T> tryRenderNodeAs() {
    return std::dynamic_pointer_cast<T>(_renderNode);
  }
  template <typename T> std::shared_ptr<T> tryOutputNodeAs() {
    return std::dynamic_pointer_cast<T>(_outputNode);
  }
  compositorpostnode_ptr_t getPostNode( size_t index ) const;
  size_t getPostNodeCount() const;

  void enablePickHud();

  constexpr static int K_NUMRENDERERS = 8;
  irenderer_ptr_t _currentRenderer();

  render_preset_data_ptr_t _renderPresetData;
  pbr::commonstuff_ptr_t _pbr_common;
  dbufcontext_ptr_t _dbufcontext_SG;
  irenderer_ptr_t _renderers[K_NUMRENDERERS];
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
  // Frame-global cull-frustum scale (unifies the old VR ORKEXP_VRCULL_MARGIN + per-drawable
  // cull_tighten). >1 widens (cull-less), 1.0 = exact view, <1 narrows (cull-more). Parsed from the
  // "CullFrustumScale" scenegraph param, stamped onto the RCFD in Scene::preRender, read by every
  // per-view cull (MeshInstCull / terrain ComputeDrawable). VR presets default this to 1.3.
  float _cullFrustumScale = 1.0f;
  hzbbuilder_ptr_t _hzb; // 1-phase occlusion HZB: built frame-end by the ForwardNode from THIS frame's
                         // depth, stamped into the RCFD in preRender for NEXT frame's per-view cull.
  int _lastGpuUpdateFrame = -1; // gpuUpdate's per-frame idempotence stamp (ctx->GetTargetFrame())
  std::vector<frame_prologue_hook_t> _framePrologueHooks;
  int _lastFramePrologueFrame = -1; // frame-prologue idempotence stamp (ctx->GetTargetFrame())
  uint32_t _pickFormat = 0;
  bool _doResizeFromMainSurface = false;
  using layer_map_t = std::map<std::string, layer_ptr_t>;
  size_t _renderer_idx = 0;
  LockedResource<layer_map_t> _layers;
  // role -> ordered list of layer names to render in that role's slot.
  // Empty / missing entry means identity (role name itself). Written
  // on the main thread in response to scene transitions; read by the
  // compositor during each frame's CPD.assignLayers build.
  std::unordered_map<std::string, std::vector<std::string>> _layerRoleOverrides;
  varmap::varmap_ptr_t _userdata;
  varmap::varmap_ptr_t _params;
  bool _dogpuinit        = true;
  bool _enableEditorLayers = false;
  Context* _boundContext = nullptr;

  asset::loadsynchro_ptr_t _loadSynchro;
  bool okToRender() const;

  // UI Surface drawables for event routing (auto-registered)
  void _registerUISurface(drawable_ptr_t drawable);
  void _unregisterUISurface(drawable_ptr_t drawable);
  const std::vector<drawable_ptr_t>& uiSurfaces() const { return _uiSurfaces; }

  // Camera lookup table (updated each frame via enqueueToRenderer)
  cameradatalut_ptr_t _cameralut;

  // Node enumeration methods
  std::vector<drawable_node_ptr_t> drawableNodesWithType(uint64_t drawable_type) const;
  std::vector<drawable_node_ptr_t> drawableNodesWithTag(uint64_t tag) const;
  std::vector<lightnode_ptr_t> lightNodes() const;
  std::vector<lightnode_ptr_t> lightNodesWithType(uint64_t light_type) const;
  std::vector<lightnode_ptr_t> lightNodesWithTag(uint64_t tag) const;
  std::vector<probenode_ptr_t> probeNodes() const;

  struct DrawItem{
    ork::lev2::DrawQueueLayer * _layer;
    drawable_node_ptr_t _drwnode;
  };

private:
  std::vector<drawable_ptr_t> _uiSurfaces;

  std::vector<DrawItem> _nodes2draw;
  bool _enable_pick_hud = false;

};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::scenegraph
