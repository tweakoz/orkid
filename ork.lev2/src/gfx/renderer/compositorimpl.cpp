////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/application/application.h>
#include "NodeCompositorFx3.h"
#include "NodeCompositorScreen.h"
#include "NodeCompositorForward.h"
#include "NodeCompositorDeferred.h"

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::CompositingData, "CompositingData");
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::lev2, EOutputRes)
DECLARE_ENUM(EOutputRes_640x480)
DECLARE_ENUM(EOutputRes_960x640)
DECLARE_ENUM(EOutputRes_1024x1024)
DECLARE_ENUM(EOutputRes_1280x720)
DECLARE_ENUM(EOutputRes_1600x1200)
DECLARE_ENUM(EOutputRes_1920x1080)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::lev2, EOutputResMult)
DECLARE_ENUM(EOutputResMult_Quarter)
DECLARE_ENUM(EOutputResMult_Half)
DECLARE_ENUM(EOutputResMult_Full)
DECLARE_ENUM(EOutputResMult_Double)
DECLARE_ENUM(EOutputResMult_Quadruple)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
BEGIN_ENUM_SERIALIZER(ork::lev2, EOutputTimeStep)
DECLARE_ENUM(EOutputTimeStep_RealTime)
DECLARE_ENUM(EOutputTimeStep_15fps)
DECLARE_ENUM(EOutputTimeStep_24fps)
DECLARE_ENUM(EOutputTimeStep_30fps)
DECLARE_ENUM(EOutputTimeStep_48fps)
DECLARE_ENUM(EOutputTimeStep_60fps)
DECLARE_ENUM(EOutputTimeStep_72fps)
DECLARE_ENUM(EOutputTimeStep_96fps)
DECLARE_ENUM(EOutputTimeStep_120fps)
DECLARE_ENUM(EOutputTimeStep_240fps)
END_ENUM_SERIALIZER()
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingData::describeX(class_t* c) {
  using namespace ork::reflect;

  RegisterProperty("Enable", &CompositingData::mbEnable);
  RegisterProperty("OutputFrames", &CompositingData::mbOutputFrames);

  RegisterMapProperty("Groups", &CompositingData::_groups);
	AnnotatePropertyForEditor<CompositingData>("Groups", "editor.factorylistbase", "CompositingGroup");

  RegisterMapProperty("Scenes", &CompositingData::_scenes);
	AnnotatePropertyForEditor<CompositingData>("Scenes", "editor.factorylistbase", "CompositingScene");

  RegisterProperty("ActiveScene", &CompositingData::_activeScene);
  RegisterProperty("ActiveItem", &CompositingData::_activeItem);

  RegisterProperty("OutputResBase", &CompositingData::mOutputBaseResolution);
  RegisterProperty("OutputResMult", &CompositingData::mOutputResMult);
  RegisterProperty("OutputFrameRate", &CompositingData::mOutputFrameRate);

  AnnotatePropertyForEditor<CompositingData>("OutputResBase", "editor.class", "ged.factory.enum");
  AnnotatePropertyForEditor<CompositingData>("OutputResMult", "editor.class", "ged.factory.enum");
  AnnotatePropertyForEditor<CompositingData>("OutputFrameRate", "editor.class", "ged.factory.enum");

  static const char* EdGrpStr = "grp://Main Enable ActiveScene ActiveItem "
                                "grp://Output OutputFrames OutputResBase OutputResMult OutputFrameRate "
                                "grp://Data Groups Scenes ";
  reflect::AnnotateClassForEditor<CompositingData>("editor.prop.groups", EdGrpStr);
}

///////////////////////////////////////////////////////////////////////////////

CompositingData::CompositingData()
    : mbEnable(true)
    , mToggle(true)
    , mbOutputFrames(false)
    , mOutputFrameRate(EOutputTimeStep_RealTime)
    , mOutputBaseResolution(EOutputRes_1280x720)
    , mOutputResMult(EOutputResMult_Full) {}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::defaultSetup(){

  auto p1 = new Fx3CompositingNode;
  auto g1 = new CompositingGroup;
  g1->_cameraName = "edcam"_pool;
  g1->_layers = "All"_pool;
  g1->_effect._effectAmount = 1.0f;
  g1->_effect._effectID = EFRAMEFX_GHOSTLY;
  p1->_writeGroup(g1);

  auto t1 = new NodeCompositingTechnique;
  auto o1 = new ScreenOutputCompositingNode;
  auto r1 = new DeferredCompositingNode;
  t1->_writeOutputNode(o1);
  t1->_writeRenderNode(r1);
  t1->_writePostFxNode(p1);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool,i1);
  _activeScene = "scene1"_pool;
  _activeItem = "item1"_pool;
  _scenes.AddSorted("scene1"_pool,s1);
}

///////////////////////////////////////////////////////////////////////////////

CompositingImpl* CompositingData::createImpl() const { return new CompositingImpl(*this); }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

CompositingImpl::CompositingImpl(const CompositingData& data)
    : _compositingData(data)
    , miActiveSceneItem(0)
    , mfTimeAccum(0.0f) {

  // on link ?
  mfTimeAccum       = 0.0f;
  mfLastTime        = 0.0f;
  miActiveSceneItem = 0;
}

CompositingImpl::~CompositingImpl() {}

///////////////////////////////////////////////////////////////////////////////

const CompositingSceneItem* CompositingImpl::compositingItem(int isceneidx, int itemidx) const {
  const CompositingSceneItem* rval               = nullptr;
  const CompositingScene* pscene                 = nullptr;
  const auto& CDATA                              = compositingData();
  const orklut<PoolString, ork::Object*>& Groups = CDATA.GetGroups();
  const orklut<PoolString, ork::Object*>& Scenes = CDATA.GetScenes();
  int inumgroups                                 = Groups.size();
  int inumscenes                                 = Scenes.size();
  if (inumscenes && isceneidx >= 0) {
    int idx = isceneidx % inumscenes;
    auto it = Scenes.find(CDATA.GetActiveScene());
    if (it != Scenes.end()) {
      ork::Object* pOBJ = it->second;
      if (pOBJ)
        pscene = rtti::autocast(pOBJ);
    }
  }
  if (pscene && itemidx >= 0) {
    const auto& Items = pscene->items();
    auto it           = Items.find(CDATA.GetActiveItem());
    if (it != Items.end()) {
      ork::Object* pOBJ = it->second;
      if (pOBJ)
        rval = rtti::autocast(pOBJ);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CompositingImpl::IsEnabled() const { return _compositingData.IsEnabled(); }

EOutputTimeStep CompositingImpl::currentFrameRateEnum() const {
  return IsEnabled() ? _compositingData.OutputFrameRate() : EOutputTimeStep_RealTime;
}

float CompositingImpl::currentFrameRate() const {
  EOutputTimeStep time_step = currentFrameRateEnum();
  float framerate           = 0.0f;
  switch (time_step) {
    case EOutputTimeStep_15fps:
      framerate = 1.0f / 15.0f;
      break;
    case EOutputTimeStep_24fps:
      framerate = 24.0f;
      break;
    case EOutputTimeStep_30fps:
      framerate = 30.0f;
      break;
    case EOutputTimeStep_48fps:
      framerate = 48.0f;
      break;
    case EOutputTimeStep_60fps:
      framerate = 60.0f;
      break;
    case EOutputTimeStep_72fps:
      framerate = 72.0f;
      break;
    case EOutputTimeStep_96fps:
      framerate = 96.0f;
      break;
    case EOutputTimeStep_120fps:
      framerate = 120.0f;
      break;
    case EOutputTimeStep_240fps:
      framerate = 240.0f;
      break;
    case EOutputTimeStep_RealTime:
    default:
      break;
  }

  return framerate;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::assemble(lev2::CompositorDrawData& drawdata) {
  auto the_renderer = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& RCFD = the_renderer.framedata();
  lev2::GfxTarget* pTARG                  = RCFD.GetTarget();
  orkstack<CompositingPassData>& cgSTACK  = drawdata.mCompositingGroupStack;
  CompositingImpl* pCMCI                  = this;

  SRect tgtrect = SRect(0, 0, pTARG->GetW(), pTARG->GetH());

  lev2::rendervar_t passdata;
  passdata.Set<orkstack<CompositingPassData>*>(&cgSTACK);
  RCFD.setUserProperty("nodes"_crc, passdata);

  /////////////////////////////////
  // bind lighting
  /////////////////////////////////

  if( _lightmgr ){ // WIP
      const CameraData* cdata = RCFD.GetCameraData();
      _lightmgr->EnumerateInFrustum(cdata->GetFrustum());
      if (_lightmgr->mLightsInFrustum.size()) {
        RCFD.SetLightManager(_lightmgr);
      }
  }

  /////////////////////////////////
  // Lock Drawable Buffer
  /////////////////////////////////

  const DrawableBuffer* DB = DrawableBuffer::BeginDbRead(7); // mDbLock.Aquire(7);
  RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));

  for( auto item : _prerendercallbacks ){
    item.second(pTARG);
  }

  if (DB) {
    _compcontext.assemble(drawdata, this);
    DrawableBuffer::EndDbRead(DB); // mDbLock.Aquire(7);
  }
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::composite(lev2::CompositorDrawData& drawdata) {
  int scene_item = 0;
  if (auto pCSI = compositingItem(0, scene_item)) {
    _compcontext._compositingTechnique = pCSI->GetTechnique();
    _compcontext.composite(drawdata, this);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

const CompositingGroup* CompositingImpl::compositingGroup(const PoolString& grpname) const {
  const CompositingGroup* rval = 0;
  if (auto sceneitem = compositingItem(0, miActiveSceneItem)) {
    auto itA = _compositingData.GetGroups().find(grpname);
    if (itA != _compositingData.GetGroups().end()) {
      ork::Object* pA = itA->second;
      rval            = rtti::autocast(pA);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::update(float dt) {

  mfLastTime = mfTimeAccum;
  mfTimeAccum += dt;

  int i0 = int(mfLastTime * 1.0f);
  int i1 = int(mfTimeAccum * 1.0f);

  if (i1 != i0)
    miActiveSceneItem++;
}

///////////////////////////////////////////////////////////////////////////////

const CompositingContext& CompositingImpl::compositingContext() const { return _compcontext; }

///////////////////////////////////////////////////////////////////////////////

CompositingContext& CompositingImpl::compositingContext() { return _compcontext; }

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
