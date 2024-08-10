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

struct CompositingScene : public ork::Object {
  DeclareConcreteX(CompositingScene, ork::Object);
public:
  CompositingScene();
  compositingsceneitem_constptr_t findItem(const std::string& named) const;
  std::unordered_map<std::string, compositingsceneitem_ptr_t> _items;
  CompositingData* _parent = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingSceneItem : public ork::Object {
  DeclareConcreteX(CompositingSceneItem, ork::Object);

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

struct CompositingTechnique : public ork::Object {
  RttiDeclareAbstract(CompositingTechnique, ork::Object);

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
class CompositingBuffer : public ork::Object {
  int miWidth;
  int miHeight;
  ork::lev2::EBufferFormat meBufferFormat;

  CompositingBuffer();
  ~CompositingBuffer();
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingContext {
  int miWidth;
  int miHeight;
  GfxMaterial3DSolid* _utilMaterial           = nullptr;
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

  inline void setSharedCameraMatrices(cameramatrices_ptr_t c){
    _shared_cameraMatrices = c;
    _cameraMatrices = c.get();
  }

  ////////////////////////////////////////////////////

  bool isStereoOnePass() const {
    return _stereo1pass;
  }
  void setStereoOnePass(bool ena) {
    _stereo1pass = ena;
  }
  const CameraMatrices* cameraMatrices() const {
    return _cameraMatrices;
  }
  static CompositingPassData FromRCFD(const RenderContextFrameData& RCFD);
  std::vector<std::string> getLayerNames() const;
  void updateCompositingSize(int w, int h);
  bool isPicking() const;
  const ViewportRect& GetDstRect() const;
  const ViewportRect& GetMrtRect() const;
  void SetDstRect(const ViewportRect& rect);
  void SetMrtRect(const ViewportRect& rect);
  void assignLayers(const std::string& layers);
  void AddLayer(const std::string& layername);
  bool HasLayer(const std::string& layername) const;
  void addStandardLayers();

  bool isValid() const {
    return _cameraMatrices or _stereoCameraMatrices;
  }

  void defaultSetup(CompositorDrawData& drawdata);

  const Frustum& monoCamFrustum() const;
  const fvec3& monoCamZnormal() const;
  fvec3 monoCamPos(const fmtx4& vizoffsetmtx) const;
  fvec2 nearAndFar() const;
  ////////////////////////////////////////////////////

  IRenderTarget* _irendertarget        = nullptr;
  bool mbDrawSource                    = true;
  std::string _cameraName;
  ork::fvec4 _clearColor;
  bool _stereo1pass                                 = false;
  const CameraMatrices* _cameraMatrices             = nullptr;
  cameramatrices_ptr_t _shared_cameraMatrices  = nullptr;
  const StereoCameraMatrices* _stereoCameraMatrices = nullptr;
  ork::svarp_t _var;
  ViewportRect mDstRect;
  ViewportRect mMrtRect;
  uint32_t _passID = 0;
  float _time = 0.0f;
  bool _ispicking = false;
  std::vector<std::string> _layernames;
  std::unordered_set<std::string> _layernameset;
  int _width = 0;
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
  fmtx4 VPL, VPR, VPM;
  fvec2 _zndc2eye;
  float _near = 0.1;
  float _far = 10.0;
  float _time = 0.0f;
};

///////////////////////////////////////////////////////////////////////////

struct CompositorDrawData {

  CompositorDrawData(rcfd_ptr_t rcfd=nullptr);

  Context* context() const;
  rcfd_ptr_t RCFD() const;
  //const RenderContextFrameData& RCFD() const;
  ViewData computeViewData() const;
  const svar16_t& property(uint64_t key) const;
  template <typename T> svar16_t property(uint64_t key) { return _properties[key]; };
  compositorimpl_ptr_t _cimpl;
  std::map<uint64_t, svar16_t> _properties;
  rcfd_ptr_t _RCFD;
};

///////////////////////////////////////////////////////////////////////////////

struct RenderPresetContext {
  compositortechnique_ptr_t _nodetek = nullptr;
  compositoroutnode_ptr_t _outputnode = nullptr;
  compositorrendernode_ptr_t _rendernode = nullptr;
};

struct RenderPresetData {
  rtgroup_ptr_t _outputGroup;
  asset::loadsynchro_ptr_t _assetSynchro;
  pbr::commonstuff_ptr_t _pbr_common;
};

using render_preset_data_ptr_t = std::shared_ptr<RenderPresetData>;

///////////////////////////////////////////////////////////////////////////////

struct CompositingData : public ork::Object {
  DeclareConcreteX(CompositingData, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingData();
  ///////////////////////////////////////////////////////

  void presetDefault();
  void presetPicking();
  void presetPickingDebug();
  RenderPresetContext presetUnlit(render_preset_data_ptr_t pdata=nullptr);
  RenderPresetContext presetDeferredPBR(render_preset_data_ptr_t pdata=nullptr);
  RenderPresetContext presetForwardPBR(render_preset_data_ptr_t pdata=nullptr);
  RenderPresetContext presetPBRVR(render_preset_data_ptr_t pdata=nullptr);
  RenderPresetContext presetForwardPBRVR(render_preset_data_ptr_t pdata=nullptr);

  compositingscene_constptr_t findScene(const std::string& named) const;

  bool IsEnabled() const {
    return mbEnable && mToggle;
  }

  void Toggle() const {
    mToggle = !mToggle;
  }

  template <typename T> std::shared_ptr<T> tryNodeTechnique(std::string scenename, //
                                                            std::string itemname) const;

  std::unordered_map<std::string, compositingscene_ptr_t> _scenes;
  mutable std::string _activeScene;
  mutable std::string _activeItem;
  mutable bool mToggle = true;
  bool mbEnable        = true;
  bool _defaultBG = true;

  int _defaultW = 100;
  int _defaultH = 100;
  int _SSAA = 0;

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

  void bindLighting(LightManager* lmgr) {
    _lightmgr = lmgr;
  }
  const LightManager* lightManager() const {
    return _lightmgr;
  }

  CompositingPassData& topCPD();
  const CompositingPassData& topCPD() const;
  const CompositingPassData& pushCPD(const CompositingPassData& cpd);
  const CompositingPassData& popCPD();
  bool hasCPD() const;

  std::string _cameraName = "spawncam";

  const CompositingData& _compositingData;
  compositordata_constptr_t _shared_compositingData;

  LightManager* _lightmgr                = nullptr;
  CameraData* _cimplcamdat               = nullptr;
  CameraMatrices* _defaultCameraMatrices = nullptr;

  float mfTimeAccum     = 0.0f;
  float mfLastTime      = 0.0f;
  int miActiveSceneItem = 0;

  //CompositingMorphable _morphable;
  compositorctx_ptr_t _compcontext;
  compositingpassdatastack_t _stack;
  std::string _name;

};

///////////////////////////////////////////////////////////////////////////////
template <typename T> std::shared_ptr<T> CompositingData::tryNodeTechnique( std::string scenename, //
                                                                            std::string itemname) const { //
  std::shared_ptr<T> rval  = nullptr;
  auto its = _scenes.find(scenename);
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
  bool _use_imgui_docking = false;
  rendervar_usermap_t _userprops;
  bool _updrendersync = false;

  acqupdatebuffer_ptr_t _updatebuffer;
  acqdrawbuffer_ptr_t _drawbuffer;
  acqdrawbuffer_lambda_t onPreCompositorRender;
  acqdrawbuffer_lambda_t onImguiRender;
  acqdrawbuffer_lambda_t onPostCompositorRender;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
