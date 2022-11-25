////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/RTTIX.inl>
#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/targetinterfaces.h>
//#include <ork/lev2/gfx/renderer/rendercontext.h>
//#include <ork/lev2/gfx/renderer/irendertarget.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

class CompositingScene : public ork::Object {
  DeclareConcreteX(CompositingScene, ork::Object);

public:
  CompositingScene();

  const orklut<PoolString, ork::Object*>& items() const {
    return _items;
  }
  orklut<PoolString, ork::Object*>& items() {
    return _items;
  }
  const CompositingSceneItem* findItem(const PoolString& named) const;

private:
  orklut<PoolString, ork::Object*> _items;
};

///////////////////////////////////////////////////////////////////////////////

struct CompositingMorphable : public dataflow::morphable {
  void WriteMorphTarget(dataflow::MorphKey name, float flerpval); // virtual
  void RecallMorphTarget(dataflow::MorphKey name);                // virtual
  void Morph1D(const dataflow::morph_event* pevent);              // virtual
};

///////////////////////////////////////////////////////////////////////////////

class CompositingTechnique : public ork::Object {
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
  CompositingTechnique* _compositingTechnique = nullptr;

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
  const ViewportRect& GetDstRect() const {
    return mDstRect;
  }
  const ViewportRect& GetMrtRect() const {
    return mMrtRect;
  }
  void SetDstRect(const ViewportRect& rect) {
    mDstRect = rect;
  }
  void SetMrtRect(const ViewportRect& rect) {
    mMrtRect = rect;
  }
  void ClearLayers();
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
  lev2::FrameTechniqueBase* mpFrameTek = nullptr;
  bool mbDrawSource                    = true;
  std::string _cameraName;
  std::string _layerName;
  ork::fvec4 _clearColor;
  bool _stereo1pass                                 = false;
  const CameraMatrices* _cameraMatrices             = nullptr;
  const StereoCameraMatrices* _stereoCameraMatrices = nullptr;
  ork::svarp_t _var;
  ViewportRect mDstRect;
  ViewportRect mMrtRect;
  uint32_t _passID = 0;
  orkset<std::string> mLayers;
  float _time = 0.0f;
  bool _ispicking = false;
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

  CompositorDrawData(lev2::FrameRenderer& renderer)
      : mFrameRenderer(renderer) {
  }

  Context* context() const;
  RenderContextFrameData& RCFD();
  const RenderContextFrameData& RCFD() const;
  ViewData computeViewData() const;
  compositorimpl_ptr_t _cimpl;
  std::map<uint64_t, svar16_t> _properties;
  lev2::FrameRenderer& mFrameRenderer;
};

///////////////////////////////////////////////////////////////////////////////

struct RenderPresetContext {
  CompositingTechnique* _nodetek     = nullptr;
  OutputCompositingNode* _outputnode = nullptr;
  RenderCompositingNode* _rendernode = nullptr;
};
///////////////////////////////////////////////////////////////////////////////

struct CompositingData : public ork::Object {
  DeclareConcreteX(CompositingData, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingData();
  ///////////////////////////////////////////////////////

  void presetDefault();
  void presetPicking();
  RenderPresetContext presetUnlit(rtgroup_ptr_t outputgrp = nullptr);
  RenderPresetContext presetDeferredPBR(rtgroup_ptr_t outputgrp = nullptr);
  RenderPresetContext presetForwardPBR(rtgroup_ptr_t outputgrp = nullptr);
  RenderPresetContext presetPBRVR();

  const orklut<PoolString, ork::Object*>& GetGroups() const {
    return _groups;
  }
  const orklut<PoolString, ork::Object*>& GetScenes() const {
    return _scenes;
  }
  orklut<PoolString, ork::Object*>& groups() {
    return _groups;
  }
  orklut<PoolString, ork::Object*>& scenes() {
    return _scenes;
  }

  const CompositingScene* findScene(const PoolString& named) const;

  PoolString& GetActiveScene() const {
    return _activeScene;
  }
  PoolString& GetActiveItem() const {
    return _activeItem;
  }

  bool IsEnabled() const {
    return mbEnable && mToggle;
  }

  void Toggle() const {
    mToggle = !mToggle;
  }

  template <typename T> T* tryNodeTechnique(PoolString scenename, PoolString itemname) const;

  orklut<PoolString, ork::Object*> _groups;
  orklut<PoolString, ork::Object*> _scenes;
  mutable PoolString _activeScene;
  mutable PoolString _activeItem;
  mutable bool mToggle = true;
  bool mbEnable        = true;

  int _defaultW = 100;
  int _defaultH = 100;

  compositorimpl_ptr_t createImpl() const;
};

///////////////////////////////////////////////////////////////////////////

struct CompositingImpl {

  CompositingImpl(const CompositingData& data);
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

  const CompositingSceneItem* compositingItem(int isceneidx, int itemidx) const;

  void update(float dt);

  void bindLighting(LightManager* lmgr) {
    _lightmgr = lmgr;
  }
  const LightManager* lightManager() const {
    return _lightmgr;
  }

  const CompositingPassData& topCPD() const;
  const CompositingPassData& pushCPD(const CompositingPassData& cpd);
  const CompositingPassData& popCPD();
  bool hasCPD() const;

private:
  const CompositingData& _compositingData;

  LightManager* _lightmgr                = nullptr;
  CameraData* _cimplcamdat               = nullptr;
  CameraMatrices* _defaultCameraMatrices = nullptr;

  float mfTimeAccum     = 0.0f;
  float mfLastTime      = 0.0f;
  int miActiveSceneItem = 0;

  CompositingMorphable _morphable;
  CompositingContext _compcontext;
  compositingpassdatastack_t _stack;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingSceneItem : public ork::Object {
  DeclareConcreteX(CompositingSceneItem, ork::Object);

public:
  CompositingSceneItem();

  CompositingTechnique* technique() const {
    return mpTechnique;
  }
  void _readTech(ork::rtti::ICastable*& val) const;
  void _writeTech(ork::rtti::ICastable* const& val);

  CompositingTechnique* mpTechnique;
};

///////////////////////////////////////////////////////////////////////////////
template <typename T> T* CompositingData::tryNodeTechnique(PoolString scenename, PoolString itemname) const {
  T* rval  = nullptr;
  auto its = _scenes.find(scenename);
  if (its != _scenes.end()) {
    auto scene = (CompositingScene*)its->second;
    auto iti   = scene->items().find(itemname);
    if (iti != scene->items().end()) {
      auto sceneitem = (CompositingSceneItem*)iti->second;
      rval           = dynamic_cast<T*>(sceneitem->mpTechnique);
    }
  }
  return rval;
}
////////////////////////////////////////////////////////////////////////////////
struct AcquiredUpdateDrawBuffer{
  DrawableBuffer* _DB = nullptr;
};
////////////////////////////////////////////////////////////////////////////////
using acqupdatebuffer_lambda_t = std::function<void(const AcquiredUpdateDrawBuffer&)>;
////////////////////////////////////////////////////////////////////////////////
struct AcquiredRenderDrawBuffer{
  AcquiredRenderDrawBuffer( rcfd_ptr_t rcfd );
  const DrawableBuffer* _DB;
  rcfd_ptr_t _RCFD;
};
////////////////////////////////////////////////////////////////////////////////
struct StandardCompositorFrame {
  using acqrf_lambda_t = std::function<void(const AcquiredRenderDrawBuffer& acq)>;

  StandardCompositorFrame(uidrawevent_constptr_t drawEvent = nullptr);
  void withAcquiredUpdateDrawBuffer(int debugcode, bool rendersync, acqupdatebuffer_lambda_t l);
  void _updateEnqueueLockedAndReleaseFrame(bool rendersync, DrawableBuffer* dbuf);
  void _updateEnqueueUnlockedAndReleaseFrame(bool rendersync, DrawableBuffer* dbuf);
  
  void attachDrawBufContext(dbufcontext_ptr_t dbc);

  void render();
  const DrawableBuffer* _tryAcquireDrawBuffer();

  rcfd_ptr_t _RCFD;
  dbufcontext_ptr_t _dbufcontextSFRAME;
  uidrawevent_constptr_t _drawEvent;
  rendertarget_uiviewport_ptr_t _rendertarget;
  compositorimpl_ptr_t compositor;
  compositingpassdata_ptr_t passdata;
  irenderer_ptr_t renderer;
  acqrf_lambda_t onPreCompositorRender;
  acqrf_lambda_t onImguiRender;
  bool _use_imgui_docking = false;
  acqrf_lambda_t onPostCompositorRender;
  rendervar_usermap_t _userprops;
  bool _updrendersync = false;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
