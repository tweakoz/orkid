////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/targetinterfaces.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

struct CompositingScene : public ::ork::Object {
  DeclareConcreteX(CompositingScene, ::ork::Object);

public:
  CompositingScene();
  compositingsceneitem_constptr_t findItem(const std::string& named) const;
  std::unordered_map<std::string, compositingsceneitem_ptr_t> _items;
  CompositingData* _parent = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingSceneItem : public ::ork::Object {
  DeclareConcreteX(CompositingSceneItem, ::ork::Object);

public:
  CompositingSceneItem();

  compositortechnique_ptr_t technique() const {
    return _technique;
  }

  template <typename T> std::shared_ptr<T> tryTechniqueAs() const {
    return std::dynamic_pointer_cast<T>(_technique);
  }

  compositortechnique_ptr_t _technique;
};

///////////////////////////////////////////////////////////////////////////////

/*struct CompositingMorphable : public dataflow::morphable {
  void WriteMorphTarget(dataflow::MorphKey name, float flerpval); // virtual
  void RecallMorphTarget(dataflow::MorphKey name);                // virtual
  void Morph1D(const dataflow::morph_event* pevent);              // virtual
};*/

///////////////////////////////////////////////////////////////////////////////

struct CompositingTechnique : public ::ork::Object {
  RttiDeclareAbstract(CompositingTechnique, ::ork::Object);

public:
  virtual void gpuInit(lev2::Context* pTARG, int w, int h) = 0;
  virtual bool assemble(CompositorDrawData& drawdata)      = 0;
  virtual void composite(CompositorDrawData& drawdata)     = 0;
};

class PickingCompositorTechnique final : public CompositingTechnique {
public:
  void gpuInit(lev2::Context* pTARG, int w, int h) override;
  bool assemble(CompositorDrawData& drawdata) override;
  void composite(CompositorDrawData& drawdata) override;
};

///////////////////////////////////////////////////////////////////////////////
class CompositingBuffer : public ::ork::Object {
  int miWidth;
  int miHeight;
  EBufferFormat meBufferFormat;

  CompositingBuffer();
  ~CompositingBuffer();
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingContext {
  int miWidth;
  int miHeight;
  GfxMaterial3DSolid* _utilMaterial               = nullptr;
  compositortechnique_ptr_t _compositingTechnique = nullptr;

  CompositingContext();
  ~CompositingContext();
  void Init(lev2::Context* pTARG);
  bool assemble(CompositorDrawData& drawdata);
  void composite(CompositorDrawData& drawdata);
  void Resize(int iW, int iH);
};

///////////////////////////////////////////////////////////////////////////

struct CompositingPassData {

  CompositingPassData() {
    _var.set<void*>(nullptr);
  }

  CompositingPassData clone() const;

  ////////////////////////////////////////////////////

  inline void setSharedCameraMatrices(cameramatrices_constptr_t c) {
    _mono_cam_matrices = c;
  }

  ////////////////////////////////////////////////////

  bool isSinglePassStereo() const {
    return _single_pass_stereo;
  }
  void setSinglePassStereo(bool ena) {
    _single_pass_stereo = ena;
  }
  cameramatrices_constptr_t cameraMatrices() const {
    return _mono_cam_matrices;
  }
  static CompositingPassData FromRCFD(const RenderContextFrameData& RCFD);
  std::vector<std::string> getLayerNames() const;
  void updateCompositingSize(int w, int h);
  bool isPicking() const;
  const ViewportRect& GetDstRect() const;
  const ViewportRect& bufferRect() const;
  void SetDstRect(const ViewportRect& rect);
  void SetMrtRect(const ViewportRect& rect);
  void assignLayers(const std::string& layers);
  void AddLayer(const std::string& layername);
  bool HasLayer(const std::string& layername) const;
  void addStandardLayers();

  bool isValid() const {
    return _mono_cam_matrices or _stereo_cam_matrices;
  }

  void defaultSetup(CompositorDrawData& drawdata);

  const Frustum& monoCamFrustum() const;
  const fvec3& monoCamZnormal() const;
  fvec3 monoCamPos(const fmtx4& vizoffsetmtx) const;
  fvec2 nearAndFar() const;
  ////////////////////////////////////////////////////

  IRenderTarget* _irendertarget = nullptr;
  bool mbDrawSource             = true;
  std::string _camera_name;
  fvec4 _clearColor;
  bool _single_pass_stereo                         = false;
  cameramatrices_constptr_t _mono_cam_matrices     = nullptr;
  const StereoCameraMatrices* _stereo_cam_matrices = nullptr;
  svarp_t _var;
  ViewportRect mDstRect;
  ViewportRect mMrtRect;
  uint32_t _passID = 0;
  float _time      = 0.0f;
  bool _ispicking  = false;
  // cascade-cull fix: set true on the CLONED CPD each sun-cascade depth pass pushes. A GPU-culled
  // drawable's indirect render reads this off the active (top) CPD to select its SHADOW survivor set
  // (union-sun-culled) instead of the eye set. False on every other pass (color, spot depth, probe)
  // -> those keep reading the eye set unchanged. Copied by clone() (member-wise cpd = *this).
  bool _sunCascadeShadowPass = false;
  // cloud-shadow (sun cookie) fill: set true on the CLONED CPD _update_sun_cookie pushes. The pass
  // consumes ALPHA only, so a material that can produce its alpha without scene lighting selects an
  // alpha-only technique off this (FxPipelinePermutation::_is_sun_cookie) instead of the full forward
  // one, whose 17 declared samplers exceed Metal's per-stage cap. Materials without that technique
  // fall through to the forward pipeline, so the pass is unchanged for them.
  bool _sunCookiePass = false;
  std::vector<std::string> _layernames;
  std::unordered_set<std::string> _layernameset;
  int _width  = 0;
  int _height = 0;

  std::string _debugName;
};

typedef std::stack<lev2::CompositingPassData> compositingpassdatastack_t;

using compositingpassdata_ptr_t = std::shared_ptr<CompositingPassData>;
///////////////////////////////////////////////////////////////////////////////

struct ViewData {
  bool _isStereo = false;
  fmtx4 _ivp[2];
  fmtx4 _v[2];
  fmtx4 _p[2];
  fvec3 _camposmono;
  fmtx4 IVPL, IVPR, IVPM;
  fmtx4 VL, VR, VM;
  fmtx4 PL, PR, PM;
  fmtx4 IPL, IPR, IPM;
  fmtx4 VPL, VPR, VPM;
  fvec2 _zndc2eye;
  float _near = 0.1;
  float _far  = 10.0;
  float _time = 0.0f;
};

///////////////////////////////////////////////////////////////////////////

struct CompositorDrawData {

  CompositorDrawData(rcfd_ptr_t rcfd = nullptr);

  Context* context() const;
  rcfd_ptr_t RCFD() const;
  // const RenderContextFrameData& RCFD() const;
  ViewData computeViewData() const;
  const svar16_t& property(uint64_t key) const;
  template <typename T> svar16_t property(uint64_t key) {
    return _properties[key];
  };
  compositorimpl_ptr_t _cimpl;
  std::map<uint64_t, svar16_t> _properties;
  rcfd_ptr_t _RCFD;
};

///////////////////////////////////////////////////////////////////////////////

struct RenderPresetContext {
  compositortechnique_ptr_t _nodetek     = nullptr;
  compositoroutnode_ptr_t _outputnode    = nullptr;
  compositorrendernode_ptr_t _rendernode = nullptr;
};

struct RenderPresetData {
  rtgroup_ptr_t _outputGroup;
  asset::loadsynchro_ptr_t _assetSynchro;
  pbr::commonstuff_ptr_t _pbr_common;
};

using render_preset_data_ptr_t = std::shared_ptr<RenderPresetData>;

///////////////////////////////////////////////////////////////////////////////

struct CompositingData : public ::ork::Object {
  DeclareConcreteX(CompositingData, ::ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingData();
  ///////////////////////////////////////////////////////

  void presetDefault();
  void presetPicking();
  void presetPickingDebug();
  RenderPresetContext presetUnlit(render_preset_data_ptr_t pdata = nullptr);
  RenderPresetContext presetDeferredPBR(render_preset_data_ptr_t pdata = nullptr);
  RenderPresetContext presetForwardPBR(render_preset_data_ptr_t pdata = nullptr);
  RenderPresetContext presetPBRVR(render_preset_data_ptr_t pdata = nullptr);
  RenderPresetContext presetForwardPBRVR(render_preset_data_ptr_t pdata = nullptr);
  RenderPresetContext presetForwardPBRVRDM(render_preset_data_ptr_t pdata = nullptr);
  // SPVR — the single-pass-stereo peer of presetForwardPBRVRDM. Same forward render
  //  node, same content, layered targets and one scene pass instead of two.
  RenderPresetContext presetForwardPBRSPVR(render_preset_data_ptr_t pdata = nullptr);

  compositingscene_constptr_t findScene(const std::string& named) const;

  bool IsEnabled() const {
    return mbEnable && mToggle;
  }

  void Toggle() const {
    mToggle = !mToggle;
  }

  template <typename T>
  std::shared_ptr<T> tryNodeTechnique(
      std::string scenename, //
      std::string itemname) const;

  std::unordered_map<std::string, compositingscene_ptr_t> _scenes;
  mutable std::string _activeScene;
  mutable std::string _activeItem;
  mutable bool mToggle = true;
  bool mbEnable        = true;
  bool _defaultBG      = true;

  int _defaultW = 100;
  int _defaultH = 100;

  compositorimpl_ptr_t createImpl() const;
};

///////////////////////////////////////////////////////////////////////////

struct CompositingImpl {

  CompositingImpl(const CompositingData& data);
  CompositingImpl(compositordata_constptr_t data);

  ~CompositingImpl();

  void gpuInit(lev2::Context* ctx);

  bool assemble(lev2::CompositorDrawData& drawdata);
  void composite(lev2::CompositorDrawData& drawdata);

  const CompositingData& compositingData() const {
    return _compositingData;
  }

  bool IsEnabled() const;

  const CompositingContext& compositingContext() const;
  CompositingContext& compositingContext();

  compositingsceneitem_ptr_t compositingItem(int isceneidx, int itemidx) const;

  void update(float dt);

  void bindLighting(lightmanager_ptr_t lmgr) {
    _lightmgr = lmgr;
  }
  lightmanager_ptr_t lightManager() const {
    return _lightmgr;
  }

  CompositingPassData& topCPD();
  const CompositingPassData& topCPD() const;
  const CompositingPassData& pushCPD(const CompositingPassData& cpd);
  const CompositingPassData& popCPD();
  bool hasCPD() const;

  std::string _camera_name = "spawncam";

  const CompositingData& _compositingData;
  compositordata_constptr_t _shared_compositingData;

  lightmanager_ptr_t _lightmgr           = nullptr;
  CameraData* _cimplcamdat               = nullptr;
  cameramatrices_ptr_t _defaultCameraMatrices = nullptr;

  float mfTimeAccum     = 0.0f;
  float mfLastTime      = 0.0f;
  int miActiveSceneItem = 0;

  // CompositingMorphable _morphable;
  compositorctx_ptr_t _compcontext;
  compositingpassdatastack_t _stack;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////
template <typename T>
std::shared_ptr<T> CompositingData::tryNodeTechnique(
    std::string scenename,        //
    std::string itemname) const { //
  std::shared_ptr<T> rval = nullptr;
  auto its                = _scenes.find(scenename);
  if (its != _scenes.end()) {
    auto scene = its->second;
    auto iti   = scene->_items.find(itemname);
    if (iti != scene->_items.end()) {
      auto sceneitem = iti->second;
      rval           = std::dynamic_pointer_cast<T>(sceneitem->_technique);
    }
  }
  return rval;
}
////////////////////////////////////////////////////////////////////////////////
struct StandardCompositorFrame {

  StandardCompositorFrame(uidrawevent_constptr_t drawEvent = nullptr);
  void withAcquiredDrawQueueForUpdate(int debugcode, bool rendersync, acqupdatebuffer_lambda_t l);
  void _updateEnqueueLockedAndReleaseFrame(bool rendersync, DrawQueue* dbuf);
  void _updateEnqueueUnlockedAndReleaseFrame(bool rendersync, DrawQueue* dbuf);

  void attachDrawQueueContext(dbufcontext_ptr_t dbc);

  void render();
  const DrawQueue* _tryAcquireDrawBuffer();
  void pushEmptyUpdateDrawBuf();

  rcfd_ptr_t _RCFD;
  dbufcontext_ptr_t _dbufcontextSFRAME;
  uidrawevent_constptr_t _drawEvent;
  rendertarget_uiviewport_ptr_t _rendertarget;
  compositorimpl_ptr_t compositor;
  compositingpassdata_ptr_t passdata;
  irenderer_ptr_t renderer;
  rendervar_usermap_t _userprops;
  bool _updrendersync = false;

  acqupdatebuffer_ptr_t _updatebuffer;
  acqdrawbuffer_ptr_t _drawbuffer;
  acqdrawbuffer_lambda_t onPreCompositorRender;
  acqdrawbuffer_lambda_t onPostCompositorRender;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
