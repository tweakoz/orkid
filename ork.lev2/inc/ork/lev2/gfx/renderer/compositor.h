////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/renderer/frametek.h>
#include <ork/lev2/gfx/gfxenv_enum.h>

namespace ork::lev2 {

class CompositingGroup;
class CompositingSceneItem;
struct CompositorDrawData;
struct CompositingContext;
struct CompositingImpl;
struct CompositingData;
struct CompositingMorphable;
class DrawableBuffer;
class LightManager;
class GfxMaterial3DSolid;
class IRenderTarget;

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
  virtual void Init(lev2::Context* pTARG, int w, int h) = 0;
  virtual bool assemble(CompositorDrawData& drawdata)   = 0;
  virtual void composite(CompositorDrawData& drawdata)  = 0;
};

class PickingCompositorTechnique : public CompositingTechnique {
public:
  void Init(lev2::Context* pTARG, int w, int h) final;
  bool assemble(CompositorDrawData& drawdata) final;
  void composite(CompositorDrawData& drawdata) final;
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
    _var.Set<void*>(nullptr);
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
  std::vector<PoolString> getLayerNames() const;
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
  void AddLayer(const PoolString& layername);
  bool HasLayer(const PoolString& layername) const;
  void addStandardLayers();

  bool isValid() const {
    return _cameraMatrices or _stereoCameraMatrices;
  }

  void defaultSetup(CompositorDrawData& drawdata);

  const Frustum& monoCamFrustum() const;
  const fvec3& monoCamZnormal() const;
  fvec3 monoCamPos(const fmtx4& vizoffsetmtx) const;
  ////////////////////////////////////////////////////

  IRenderTarget* _irendertarget        = nullptr;
  const CompositingGroup* mpGroup      = nullptr;
  lev2::FrameTechniqueBase* mpFrameTek = nullptr;
  bool mbDrawSource                    = true;
  const PoolString* mpCameraName       = nullptr;
  const PoolString* mpLayerName        = nullptr;
  ork::fvec4 _clearColor;
  bool _stereo1pass                                 = false;
  const CameraMatrices* _cameraMatrices             = nullptr;
  const StereoCameraMatrices* _stereoCameraMatrices = nullptr;
  ork::svarp_t _var;
  ViewportRect mDstRect;
  ViewportRect mMrtRect;
  uint32_t _passID = 0;
  orkset<PoolString> mLayers;
  bool _ispicking = false;
};

typedef std::stack<lev2::CompositingPassData> compositingpassdatastack_t;

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
  CompositingImpl* _cimpl = nullptr;
  std::map<uint64_t, svar16_t> _properties;
  lev2::FrameRenderer& mFrameRenderer;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingData : public ork::Object {
  DeclareConcreteX(CompositingData, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingData();
  ///////////////////////////////////////////////////////

  void presetDefault();
  void presetPicking();
  void presetPBR();
  void presetPBRVR();

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
  mutable bool mToggle;
  bool mbEnable;

  CompositingImpl* createImpl() const;
};

using compositordata_ptr_t = std::shared_ptr<CompositingData>;

///////////////////////////////////////////////////////////////////////////

class CompositingImpl {
public:
  CompositingImpl(const CompositingData& data);
  ~CompositingImpl();

  bool assemble(lev2::CompositorDrawData& drawdata);
  void composite(lev2::CompositorDrawData& drawdata);

  const CompositingData& compositingData() const {
    return _compositingData;
  }

  bool IsEnabled() const;

  const CompositingContext& compositingContext() const;
  CompositingContext& compositingContext();

  const CompositingSceneItem* compositingItem(int isceneidx, int itemidx) const;

  const CompositingGroup* compositingGroup(const PoolString& grpname) const;

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

  float mfTimeAccum;
  float mfLastTime;
  LightManager* _lightmgr  = nullptr;
  CameraData* _cimplcamdat = nullptr;
  CompositingMorphable _morphable;
  CameraMatrices* _defaultCameraMatrices = nullptr;
  int miActiveSceneItem;
  CompositingContext _compcontext;
  compositingpassdatastack_t _stack;
};

using compositorimpl_ptr_t = std::shared_ptr<CompositingImpl>;

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

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
