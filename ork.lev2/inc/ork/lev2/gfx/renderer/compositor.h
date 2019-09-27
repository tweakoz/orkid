////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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

  const orklut<PoolString, ork::Object*>& items() const { return _items; }
  orklut<PoolString, ork::Object*>& items() { return _items; }

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
  virtual void Init(lev2::GfxTarget* pTARG, int w, int h) = 0;
  virtual bool assemble(CompositorDrawData& drawdata) = 0;
  virtual void composite(CompositorDrawData& drawdata) = 0;
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
  GfxMaterial3DSolid* _utilMaterial = nullptr;
  CompositingTechnique* _compositingTechnique = nullptr;

  CompositingContext();
  ~CompositingContext();
  void Init(lev2::GfxTarget* pTARG);
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

  bool isStereoOnePass() const { return _stereo1pass; }
  void setStereoOnePass(bool ena) { _stereo1pass=ena; }
  const CameraMatrices* cameraMatrices() const { return _cameraMatrices; }
  void setCameraMatrices(const CameraMatrices* m) { _cameraMatrices = m; }
  static CompositingPassData FromRCFD(const RenderContextFrameData& RCFD);
  std::vector<PoolString> getLayerNames() const;
  void updateCompositingSize(int w, int h);
  bool isPicking() const;
  const SRect& GetDstRect() const { return mDstRect; }
  const SRect& GetMrtRect() const { return mMrtRect; }
  void SetDstRect(const SRect& rect) { mDstRect = rect; }
  void SetMrtRect(const SRect& rect) { mMrtRect = rect; }
  void ClearLayers();
  void AddLayer(const PoolString& layername);
  bool HasLayer(const PoolString& layername) const;
  void addStandardLayers();

  bool isValid() const { return _cameraMatrices or _stereoCameraMatrices; }

  const Frustum& monoCamFrustum() const;
  const fvec3& monoCamZnormal() const;

  ////////////////////////////////////////////////////

  IRenderTarget* _irendertarget = nullptr;
  const CompositingGroup* mpGroup = nullptr;
  lev2::FrameTechniqueBase* mpFrameTek = nullptr;
  bool mbDrawSource = true;
  const PoolString* mpCameraName = nullptr;
  const PoolString* mpLayerName = nullptr;
  ork::fvec4 _clearColor;
  bool _stereo1pass = false;
  const CameraMatrices* _cameraMatrices = nullptr;
  const StereoCameraMatrices* _stereoCameraMatrices = nullptr;
  ork::svarp_t _var;
  SRect mDstRect;
  SRect mMrtRect;
  orkset<PoolString> mLayers;
};

typedef std::stack<lev2::CompositingPassData> compositingpassdatastack_t;

///////////////////////////////////////////////////////////////////////////

struct CompositorDrawData {
  GfxTarget* target() const;
  CompositingImpl* _cimpl = nullptr;
  std::map<uint64_t,svar16_t> _properties;
  lev2::FrameRenderer& mFrameRenderer;

  CompositorDrawData(lev2::FrameRenderer& renderer) : mFrameRenderer(renderer) {}
};

///////////////////////////////////////////////////////////////////////////////

class CompositingData : public ork::Object {
  DeclareConcreteX(CompositingData, ork::Object);

public:
  ///////////////////////////////////////////////////////
  CompositingData();
  ///////////////////////////////////////////////////////

  void defaultSetup();

  const orklut<PoolString, ork::Object*>& GetGroups() const { return _groups; }
  const orklut<PoolString, ork::Object*>& GetScenes() const { return _scenes; }

  PoolString& GetActiveScene() const { return _activeScene; }
  PoolString& GetActiveItem() const { return _activeItem; }

  bool IsEnabled() const { return mbEnable && mToggle; }

  void Toggle() const { mToggle = !mToggle; }

  orklut<PoolString, ork::Object*> _groups;
  orklut<PoolString, ork::Object*> _scenes;
  mutable PoolString _activeScene;
  mutable PoolString _activeItem;
  mutable bool mToggle;
  bool mbEnable;

  CompositingImpl* createImpl() const;
};

///////////////////////////////////////////////////////////////////////////

class CompositingImpl {
public:

  CompositingImpl(const CompositingData& data);
  ~CompositingImpl();

  bool assemble(lev2::CompositorDrawData& drawdata);
  void composite(lev2::CompositorDrawData& drawdata);

  const CompositingData& compositingData() const { return _compositingData; }

  bool IsEnabled() const;

  const CompositingContext& compositingContext() const;
  CompositingContext& compositingContext();

  const CompositingSceneItem* compositingItem(int isceneidx, int itemidx) const;

  const CompositingGroup* compositingGroup(const PoolString& grpname) const;

  void update(float dt);

  void bindLighting(LightManager* lmgr) { _lightmgr=lmgr; }

  const CompositingPassData& topCPD() const;
  const CompositingPassData& pushCPD(const CompositingPassData& cpd);
  const CompositingPassData& popCPD();

private:
  const CompositingData& _compositingData;


  float mfTimeAccum;
  float mfLastTime;
  LightManager* _lightmgr = nullptr;
  CameraData* _cimplcamdat = nullptr;
  CompositingMorphable _morphable;
  CameraMatrices* _defaultCameraMatrices = nullptr;
  int miActiveSceneItem;
  CompositingContext _compcontext;
  compositingpassdatastack_t _stack;
};

///////////////////////////////////////////////////////////////////////////////

class CompositingSceneItem : public ork::Object {
  DeclareConcreteX(CompositingSceneItem, ork::Object);

public:
  CompositingSceneItem();

  CompositingTechnique* GetTechnique() const { return mpTechnique; }
  void _readTech(ork::rtti::ICastable*& val) const;
  void _writeTech(ork::rtti::ICastable* const& val);


  CompositingTechnique* mpTechnique;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
