////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include "NodeCompositor/NodeCompositorDeferred.h"
#include "NodeCompositor/NodeCompositorForward.h"
#include "NodeCompositor/NodeCompositorFx3.h"
#include "NodeCompositor/NodeCompositorScaleBias.h"
#include "NodeCompositor/NodeCompositorScreen.h"
#include "NodeCompositor/NodeCompositorVr.h"
#include <ork/application/application.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::CompositingData, "CompositingData");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingData::describeX(class_t* c) {
  using namespace ork::reflect;

  RegisterProperty("Enable", &CompositingData::mbEnable);

  RegisterMapProperty("Groups", &CompositingData::_groups);
  AnnotatePropertyForEditor<CompositingData>("Groups", "editor.factorylistbase", "CompositingGroup");

  RegisterMapProperty("Scenes", &CompositingData::_scenes);
  AnnotatePropertyForEditor<CompositingData>("Scenes", "editor.factorylistbase", "CompositingScene");

  RegisterProperty("ActiveScene", &CompositingData::_activeScene);
  RegisterProperty("ActiveItem", &CompositingData::_activeItem);

  static const char* EdGrpStr = "grp://Main Enable ActiveScene ActiveItem "
                                "grp://Data Groups Scenes ";
  reflect::AnnotateClassForEditor<CompositingData>("editor.prop.groups", EdGrpStr);
}

///////////////////////////////////////////////////////////////////////////////

CompositingData::CompositingData()
    : mbEnable(true)
    , mToggle(true) {}

//////////////////////////////////////////////////////////////////////////////

void CompositingData::defaultSetup() {

  auto p1 = new ScaleBiasCompositingNode;

  auto t1 = new NodeCompositingTechnique;
  // auto o1 = new ScreenOutputCompositingNode;
  auto o1 = new VrCompositingNode;
  auto r1 = new DeferredCompositingNode;
  t1->_writeOutputNode(o1);
  t1->_writeRenderNode(r1);
  // t1->_writePostFxNode(p1);

  auto s1 = new CompositingScene;
  auto i1 = new CompositingSceneItem;
  i1->_writeTech(t1);
  s1->items().AddSorted("item1"_pool, i1);
  _activeScene = "scene1"_pool;
  _activeItem  = "item1"_pool;
  _scenes.AddSorted("scene1"_pool, s1);
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

  _cimplcamdat = new CameraData;
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

///////////////////////////////////////////////////////////////////////////////

bool CompositingImpl::assemble(lev2::CompositorDrawData& drawdata) {
  bool rval                              = false;
  auto& ddprops                          = drawdata._properties;
  auto the_renderer                      = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& RCFD     = the_renderer.framedata();
  lev2::GfxTarget* target                = RCFD.GetTarget();
  orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
  drawdata._cimpl                        = this;

  float aspectratio = float(target->GetW()) / float(target->GetH());
  // todo - compute CameraMatrices per rendertarget/pass !
  auto& CAMCCTX = RCFD.cameraMatrices();

  CAMCCTX._aspectRatio = aspectratio;

  SRect tgtrect = SRect(0, 0, target->GetW(), target->GetH());

  lev2::rendervar_t passdata;
  passdata.Set<orkstack<CompositingPassData>*>(&cgSTACK);
  RCFD.setUserProperty("nodes"_crc, passdata);

  /////////////////////////////////////////////////////////
  // bind compositing technique
  /////////////////////////////////////////////////////////
  int scene_item = 0;
  if (auto item = compositingItem(0, scene_item)) {
    _compcontext._compositingTechnique = item->GetTechnique();
  }
  /////////////////////////////////
  // bind lighting
  /////////////////////////////////

  if (_lightmgr) { // WIP
    const auto& cmatrices = RCFD.cameraMatrices();
    _lightmgr->EnumerateInFrustum(cmatrices.GetFrustum());
    if (_lightmgr->mLightsInFrustum.size()) {
      RCFD.SetLightManager(_lightmgr);
    }
  }

  /////////////////////////////////
  // Lock Drawable Buffer
  /////////////////////////////////

  const DrawableBuffer* DB = DrawableBuffer::acquireReadDB(7); // mDbLock.Aquire(7);
  RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));

  int primarycamindex = ddprops["primarycamindex"_crcu].Get<int>();
  int cullcamindex    = ddprops["cullcamindex"_crcu].Get<int>();

  if (DB) {

    /////////////////////////////////////////////////////////////////////////////
    // default camera selection (todo: hoist to cimpl so it can be shared across output nodes)
    /////////////////////////////////////////////////////////////////////////////

    auto spncam = (CameraData*)DB->cameraData("spawncam"_pool);
    auto l2cam  = spncam->getEditorCamera();
    if (l2cam) {
      spncam->computeMatrices(CAMCCTX);
      l2cam->_camcamdata.BindGfxTarget(target);
      //_tempcamdat = l2cam->mCameraData;
      ddprops["selcamdat"_crcu].Set<const CameraData*>(spncam);
    }

    /////////////////////////////////////////////////////////////////////////////

    DB->invokePreRenderCallbacks(RCFD);
    rval = _compcontext.assemble(drawdata);
    DrawableBuffer::releaseReadDB(DB); // mDbLock.Aquire(7);
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::composite(lev2::CompositorDrawData& drawdata) {
  int scene_item = 0;
  if (auto pCSI = compositingItem(0, scene_item)) {
    _compcontext._compositingTechnique = pCSI->GetTechnique();
    _compcontext.composite(drawdata);
  }
}

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
