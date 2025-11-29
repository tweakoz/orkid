////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/ui/viewport_scenegraph.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/ui/group.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ui::SceneGraphViewport, "ui::SceneGraphViewport");

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphViewport::SceneGraphViewport(const std::string& name, int x, int y, int w, int h)
    : Viewport(name, x, y, w, h, fvec4(1, 0, 1, 1), 1.0f) {
  _flipY = false;

  // Create embedded UI context for 3D UI surfaces
  _embeddedUiContext = std::make_shared<Context>();
  _embeddedUiContext->_id = "sgvp_embedded";
  // The context needs a top group to route events through
  _embeddedUiContext->makeTop<Group>("embedded_root", 0, 0, 1, 1);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::_doGpuInit(lev2::Context* context) {
  Viewport::_doGpuInit(context);
  _outputnode = std::make_shared<lev2::RtGroupOutputCompositingNode>(_rtgroup);
  //_outputnode->_flipY = true;
  _rtgroup->_name = FormatString("ui::SceneGraphViewport<%p>", (void*)this);
  _outputnode->setSuperSample(_supersample);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::forkDB() {

  _override_acqdbuf = std::make_shared<lev2::AcquiredDrawQueueForRendering>();
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::bindSceneGraph(lev2::scenegraph::scene_ptr_t sg) {
  _scenegraph = sg;
  if (sg->_params->hasKey("ssaa")) {
    auto& ssaa = sg->_params->valueForKey("ssaa");
    if (auto as_ssaa = ssaa.tryAs<int>()) {
      _supersample = as_ssaa.value();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  if (_scenegraph) {

    ////////////////////////////////////////////////////
    // in this case we already have a AcquiredDrawQueueForRendering!
    //  provided by ezapp_topwidget enableUiDraw()
    ////////////////////////////////////////////////////

    auto acqbuf = drwev->_acqdbuf;

    if (_override_acqdbuf) {
      const lev2::DrawQueue* DB = nullptr;
      while (nullptr == DB) {
        DB = _scenegraph->_dbufcontext_SG->acquireForReadLocked();
        if(DB==nullptr){
            ::usleep(100);
        }
      }
      auto WDB = (lev2::DrawQueue*)DB;
      WDB->setUserProperty("vpID"_crcu, _userID);
      auto RCFD                = drwev->_acqdbuf->_RCFD;
      _override_acqdbuf->_RCFD = RCFD;
      _override_acqdbuf->_DB   = DB;
      acqbuf                   = _override_acqdbuf;
    }

    auto cimpl          = _scenegraph->_compositorImpl;
    cimpl->_camera_name = _cameraname;
    if (_decouple_from_ui_size) {
      cimpl->_compcontext->Resize(_decoupled_width, _decoupled_height);
    } else {
      cimpl->_compcontext->Resize(width(), height());
    }
    auto comptek = _scenegraph->_compositorTechnique;

    auto orig_onode                  = comptek->_outputNode;
    comptek->_renderNode->_bufferKey = (uint64_t)this;

    comptek->_outputNode = _outputnode;

    _scenegraph->_renderWithAcquiredDrawQueueForRendering(acqbuf);

    comptek->_outputNode = orig_onode;

    ////////////////////////////////////////////////////
    if (_override_acqdbuf) {
      _scenegraph->_dbufcontext_SG->releaseFromReadLocked(_override_acqdbuf->_DB);
      _override_acqdbuf->_DB = nullptr;
    }
    ////////////////////////////////////////////////////
  }
}
// Surface::DoDraw()

///////////////////////////////////////////////////////////////////////////////

HandlerResult SceneGraphViewport::_routeToEmbeddedUiSurfaces(event_constptr_t ev) {
  if (!_scenegraph) {
    return HandlerResult();
  }

  const auto& uiSurfaces = _scenegraph->uiSurfaces();
  if (uiSurfaces.empty()) {
    return HandlerResult();
  }

  auto cameralut = _scenegraph->_cameralut;
  if (!cameralut) {
    return HandlerResult();
  }

  auto camera = cameralut->find(_cameraname);
  if (!camera) {
    return HandlerResult();
  }

  float aspect = (height() > 0) ? float(width()) / float(height()) : 1.0f;
  auto camMtx = camera->computeMatrices(aspect);

  // Generate world-space ray from screen coordinates
  float nx = (2.0f * ev->miX / float(width())) - 1.0f;
  float ny = 1.0f - (2.0f * ev->miY / float(height()));

  fvec4 nearNDC(nx, ny, -1.0f, 1.0f);
  fvec4 farNDC(nx, ny, 1.0f, 1.0f);

  auto invVP = (camMtx._pmatrix * camMtx._vmatrix).inverse();
  fvec4 nearWorld4 = nearNDC.transform(invVP);
  fvec4 farWorld4 = farNDC.transform(invVP);

  fvec3 nearWorld = nearWorld4.xyz() / nearWorld4.w;
  fvec3 farWorld = farWorld4.xyz() / farWorld4.w;
  fvec3 rayDir = (farWorld - nearWorld).normalized();
  fray3 worldRay(nearWorld, rayDir);

  // Test each UI surface for intersection
  for (auto& drawable : uiSurfaces) {
    auto impl = lev2::getUISurfaceRenderImpl(drawable);
    if (!impl) continue;

    // Check if layoutSurface is valid before ray testing
    auto layoutSurface = impl->_layoutSurface;
    if (!layoutSurface) continue;

    fvec2 surfaceUV;
    fvec3 worldHitPos;

    if (impl->rayIntersect(worldRay, camMtx, surfaceUV, worldHitPos)) {
      // Hit! Transform UV [0,1] to LayoutSurface pixel coordinates

      int surfaceW = layoutSurface->width();
      int surfaceH = layoutSurface->height();

      // Create transformed event with surface-local coordinates
      auto transformedEv = std::make_shared<Event>();
      *transformedEv = *ev;
      transformedEv->miX = int(surfaceUV.x * surfaceW);
      transformedEv->miY = int(surfaceUV.y * surfaceH);

      // Route through the LayoutSurface's widget tree
      auto result = layoutSurface->handleUiEvent(transformedEv);
      if (result.wasHandled()) {
        return result;
      }
    }
  }

  return HandlerResult();
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult SceneGraphViewport::DoOnUiEvent(event_constptr_t ev) {
  // 1. Try embedded UI surfaces first
  auto result = _routeToEmbeddedUiSurfaces(ev);
  if (result.wasHandled()) {
    return result;
  }

  // 2. Camera handler (e.g., EzUiCam)
  if (_camera_evhandler) {
    result = _camera_evhandler(ev);
    if (result.wasHandled()) {
      return result;
    }
  }

  // 3. Default: not handled
  return HandlerResult();
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
