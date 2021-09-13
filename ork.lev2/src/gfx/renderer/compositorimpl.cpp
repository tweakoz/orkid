////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/profiling.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

compositorimpl_ptr_t CompositingData::createImpl() const {
  return std::make_shared<CompositingImpl>(*this);
}

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
  CompositingPassData topcpd;
  _stack.push(topcpd);

  _cimplcamdat = new CameraData;

  _defaultCameraMatrices = new CameraMatrices;
}

CompositingImpl::~CompositingImpl() {
}

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

bool CompositingImpl::IsEnabled() const {
  return _compositingData.IsEnabled();
}

///////////////////////////////////////////////////////////////////////////////

bool CompositingImpl::assemble(lev2::CompositorDrawData& drawdata) {
  EASY_BLOCK("assemble-ci");
  auto& ddprops                      = drawdata._properties;
  auto the_renderer                  = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& RCFD = the_renderer.framedata();
  lev2::Context* target              = RCFD.GetTarget();

  float aspectratio = target->mainSurfaceAspectRatio();

  // todo - compute CameraMatrices per rendertarget/pass !

  // lev2::rendervar_t passdata;
  // passdata.set<orkstack<CompositingPassData>*>(&cgSTACK);
  // RCFD.setUserProperty("nodes"_crc, passdata);

  /////////////////////////////////////////////////////////
  // bind compositing technique
  /////////////////////////////////////////////////////////
  int scene_item = 0;
  if (auto item = compositingItem(0, scene_item)) {
    _compcontext._compositingTechnique = item->technique();
  }

  /////////////////////////////////
  // Lock Drawable Buffer
  /////////////////////////////////

  int primarycamindex = ddprops["primarycamindex"_crcu].get<int>();
  int cullcamindex    = ddprops["cullcamindex"_crcu].get<int>();
  auto DB             = ddprops["DB"_crcu].get<const DrawableBuffer*>();

  /////////////////////////////////////////////////////////////////////////////
  // default camera selection
  //  todo - create actual camera mgr and select default camera there
  /////////////////////////////////////////////////////////////////////////////

  auto spncam = (CameraData*)DB->cameraData("spawncam"_pool);

  target->debugMarker(FormatString("spncam<%p>", spncam));

  if (spncam) {
    (*_defaultCameraMatrices) = spncam->computeMatrices(aspectratio);
  }

  target->debugMarker(FormatString("defcammtx<%p>", _defaultCameraMatrices));
  ddprops["defcammtx"_crcu].set<const CameraMatrices*>(_defaultCameraMatrices);

  if (spncam and spncam->getUiCamera()) {
    // spncam->computeMatrices(CAMCCTX);
    // l2cam->_camcamdata.BindContext(target);
    //_tempcamdat = l2cam->mCameraData;
    target->debugMarker(FormatString("seleditcam<%p>", spncam));
    ddprops["seleditcam"_crcu].set<const CameraData*>(spncam);
  }

  /////////////////////////////////////////////////////////////////////////////

  DB->invokePreRenderCallbacks(RCFD);
  return _compcontext.assemble(drawdata);
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::composite(lev2::CompositorDrawData& drawdata) {
  int scene_item = 0;
  if (auto pCSI = compositingItem(0, scene_item)) {
    _compcontext._compositingTechnique = pCSI->technique();
    _compcontext.composite(drawdata);
  }
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

bool CompositingImpl::hasCPD() const {
  return (_stack.size() != 0);
}
const CompositingPassData& CompositingImpl::topCPD() const {
  return _stack.top();
}
const CompositingPassData& CompositingImpl::pushCPD(const CompositingPassData& cpd) {
  const CompositingPassData& prev = topCPD();
  _stack.push(cpd);
  return prev;
}
const CompositingPassData& CompositingImpl::popCPD() {
  _stack.pop();
  return _stack.top();
}

///////////////////////////////////////////////////////////////////////////////

const CompositingContext& CompositingImpl::compositingContext() const {
  return _compcontext;
}

///////////////////////////////////////////////////////////////////////////////

CompositingContext& CompositingImpl::compositingContext() {
  return _compcontext;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
