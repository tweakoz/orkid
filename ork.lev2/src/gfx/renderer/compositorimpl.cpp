////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/application/application.h>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/profiling.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

CompositingImpl::CompositingImpl(const CompositingData& data)
    : _compositingData(data) {
  // on link ?
  mfTimeAccum       = 0.0f;
  mfLastTime        = 0.0f;
  miActiveSceneItem = 0;
  CompositingPassData topcpd;
  _stack.push(topcpd);

  _cimplcamdat = new CameraData;

  _defaultCameraMatrices = new CameraMatrices;

  _compcontext = std::make_shared<CompositingContext>();
  _compcontext->Resize(data._defaultW,data._defaultH);
}

CompositingImpl::CompositingImpl(compositordata_constptr_t data)
  : CompositingImpl(*data) {

  _shared_compositingData = data;
}

CompositingImpl::~CompositingImpl() {
}

///////////////////////////////////////////////////////////////////////////////

compositingsceneitem_ptr_t CompositingImpl::compositingItem(int isceneidx, int itemidx) const {
  compositingsceneitem_ptr_t rval               = nullptr;
  compositingscene_constptr_t pscene                 = nullptr;
  const auto& CDATA                              = compositingData();
  const auto& scenemap = CDATA._scenes;
  auto it = scenemap.begin();
  while(isceneidx >= 0) {
      if (it == scenemap.end()) {
      }
      else{
        pscene = it->second;
        it++;
      }
      isceneidx--;
  }
  if (pscene && itemidx >= 0) {
    const auto& scene_items = pscene->_items;
    auto it           = scene_items.find(CDATA._activeItem);
    if (it != scene_items.end()) {
      rval = it->second;
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CompositingImpl::IsEnabled() const {
  return _compositingData.IsEnabled();
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::gpuInit(lev2::Context* ctx){
  int scene_item = 0;
  _compcontext->Init(ctx);
  if (auto item = compositingItem(0, scene_item)) {
    _compcontext->_compositingTechnique = item->technique();
    int w = _compcontext->miWidth;
    int h = _compcontext->miHeight;
    _compcontext->_compositingTechnique->gpuInit(ctx,w,h);
  }

}
///////////////////////////////////////////////////////////////////////////////

bool CompositingImpl::assemble(lev2::CompositorDrawData& drawdata) {
  EASY_BLOCK("assemble-ci", profiler::colors::Red);
  auto& ddprops                      = drawdata._properties;
    auto RCFD = drawdata.RCFD();
  lev2::Context* target              = RCFD->GetTarget();

  float aspectratio = float(_compcontext->miWidth)/float(_compcontext->miHeight);

  //printf( "CI W<%d> H<%d> aspect<%g>\n", _compcontext->miWidth, _compcontext->miHeight, aspectratio );

  // todo - compute CameraMatrices per rendertarget/pass !

  // lev2::rendervar_t passdata;
  // passdata.set<orkstack<CompositingPassData>*>(&cgSTACK);
  // RCFD.setUserProperty("nodes"_crc, passdata);

  /////////////////////////////////////////////////////////
  // bind compositing technique
  /////////////////////////////////////////////////////////
  int scene_item = 0;
  if (auto item = compositingItem(0, scene_item)) {
    _compcontext->_compositingTechnique = item->technique();
  }

  /////////////////////////////////
  // Lock Drawable Buffer
  /////////////////////////////////

  int primarycamindex = ddprops["primarycamindex"_crcu].get<int>();
  int cullcamindex    = ddprops["cullcamindex"_crcu].get<int>();
  auto DB             = ddprops["DB"_crcu].get<const DrawQueue*>();

  /////////////////////////////////////////////////////////////////////////////
  // default camera selection
  //  todo - create actual camera mgr and select default camera there
  /////////////////////////////////////////////////////////////////////////////

  auto the_camera = DB->cameraData(_cameraName);

  //printf( "CAMNAME<%s> CAM<%p>\n", _cameraName.c_str(), (void*) the_camera.get() );

  target->debugMarker(FormatString("the_camera<%p>", (void*) the_camera.get()));

  if (the_camera) {
    (*_defaultCameraMatrices) = the_camera->computeMatrices(aspectratio);
  }

  target->debugMarker(FormatString("defcammtx<%p>", _defaultCameraMatrices));
  ddprops["defcammtx"_crcu].set<const CameraMatrices*>(_defaultCameraMatrices);

  if (the_camera and the_camera->getUiCamera()) {
    target->debugMarker(FormatString("seleditcam<%p>", (void*) the_camera.get() ));
    ddprops["seleditcam"_crcu].set<cameradata_constptr_t>(the_camera);
  }

  /////////////////////////////////////////////////////////////////////////////

  DB->invokePreRenderCallbacks(RCFD);
  return _compcontext->assemble(drawdata);
}

///////////////////////////////////////////////////////////////////////////////

void CompositingImpl::composite(lev2::CompositorDrawData& drawdata) {
  int scene_item = 0;
  if (auto pCSI = compositingItem(0, scene_item)) {
    _compcontext->_compositingTechnique = pCSI->technique();
    _compcontext->composite(drawdata);
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
CompositingPassData& CompositingImpl::topCPD() {
  auto& top = _stack.top();
  return (CompositingPassData&)top;
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
  return *_compcontext;
}

///////////////////////////////////////////////////////////////////////////////

CompositingContext& CompositingImpl::compositingContext() {
  return *_compcontext;
}

///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
