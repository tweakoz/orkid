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
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <set>

INSTANTIATE_TRANSPARENT_RTTI(ork::ui::SceneGraphViewport, "ui::SceneGraphViewport");

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

static std::set<SceneGraphViewport*> _active_sgviewports;

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

  _active_sgviewports.insert(this);
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphViewport::~SceneGraphViewport() {
  _active_sgviewports.erase(this);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::gpuUpdateAll(lev2::Context* ctx) {
  for (auto* vp : _active_sgviewports) {
    if (vp->_scenegraph) {
      vp->_scenegraph->gpuUpdate(ctx);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::_doGpuInit(lev2::Context* context) {
  Viewport::_doGpuInit(context);
  _outputnode = std::make_shared<lev2::RtGroupOutputCompositingNode>(_rtgroup);
  //_outputnode->_flipY = true;
  static int vpcount = 0;
  _rtgroup->_name = FormatString("ui.sgvp.%d", vpcount++);
  _outputnode->setSuperSample(_supersample);
  if( _scenegraph ) {
    _scenegraph->gpuInit(context);
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::forkDB() {

  _override_acqdbuf = std::make_shared<lev2::AcquiredDrawQueueForRendering>();
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::bindSceneGraph(lev2::scenegraph::scene_ptr_t sg) {
  _scenegraph = sg;
  if (sg && sg->_params->hasKey("ssaa")) {
    auto& ssaa = sg->_params->valueForKey("ssaa");
    if (auto as_ssaa = ssaa.tryAs<int>()) {
      _supersample = as_ssaa.value();
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::bindManipController(lev2::editor::manipcontroller_ptr_t mc) {
  _manipController = mc;
  if (mc) {
    _manip_evhandler = [mc](event_constptr_t ev) -> HandlerResult {
      return mc->handleEvent(ev);
    };
  } else {
    _manip_evhandler = nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  // Pre-render callback (runs inside frame context)
  if (_preRenderCallback) {
    _preRenderCallback(drwev->GetTarget());
  }

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

    auto cimpl = _scenegraph->_compositorImpl;
    if (!cimpl) return;
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
    if(0)printf("_routeToEmbeddedUiSurfaces: no scenegraph\n");
    return HandlerResult();
  }

  const auto& uiSurfaces = _scenegraph->uiSurfaces();
  if (uiSurfaces.empty()) {
    if(0)printf("_routeToEmbeddedUiSurfaces: no uiSurfaces\n");
    return HandlerResult();
  }

  auto cameralut = _scenegraph->_cameralut;
  if (!cameralut) {
    if(0)printf("_routeToEmbeddedUiSurfaces: no cameralut\n");
    return HandlerResult();
  }

  auto camera = cameralut->find(_cameraname);
  if (!camera) {
    if(0)printf("_routeToEmbeddedUiSurfaces: camera '%s' not found\n", _cameraname.c_str());
    return HandlerResult();
  }

  float aspect = (height() > 0) ? float(width()) / float(height()) : 1.0f;
  auto camMtx = camera->computeMatrices(aspect);

  // Generate world-space ray from screen coordinates
  // Screen coords: (0,0) is top-left, (width,height) is bottom-right
  // Vulkan NDC: Y is flipped compared to OpenGL
  // NDC Y: -1 = top, +1 = bottom (Vulkan convention)
  float nx = (2.0f * ev->miX / float(width())) - 1.0f;
  float ny = (2.0f * ev->miY / float(height())) - 1.0f;

  // Unproject near plane point (z=-1 in NDC) to get ray origin on near plane
  fvec4 nearNDC(nx, ny, -1.0f, 1.0f);

  // For column vectors: clip = P * V * worldPos
  // So to unproject: worldPos = inv(P*V) * clip
  auto VP = camMtx._pmatrix * camMtx._vmatrix;
  auto invVP = VP.inverse();
  fvec4 nearWorld4 = nearNDC.transform(invVP);
  fvec3 nearWorld = nearWorld4.xyz() / nearWorld4.w;

  // Get camera position from inverse view matrix (translation component)
  auto invV = camMtx._vmatrix.inverse();
  fvec3 camPos(invV.elemXY(3, 0), invV.elemXY(3, 1), invV.elemXY(3, 2));

  // Ray goes from camera through the near plane point
  fvec3 rayDir = (nearWorld - camPos).normalized();
  fray3 worldRay(camPos, rayDir);

  if(0)printf("  ray: camPos=(%f,%f,%f) nearWorld=(%f,%f,%f) dir=(%f,%f,%f)\n",
         camPos.x, camPos.y, camPos.z,
         nearWorld.x, nearWorld.y, nearWorld.z,
         rayDir.x, rayDir.y, rayDir.z);

  if(0)printf("_routeToEmbeddedUiSurfaces: testing %zu surfaces\n", uiSurfaces.size());

  // Test each UI surface for intersection
  for (auto& drawable : uiSurfaces) {
    auto impl = lev2::getUISurfaceRenderImpl(drawable);
    if (!impl) {
      if(0)printf("  drawable has no impl\n");
      continue;
    }

    // Check if layoutSurface is valid before ray testing
    auto layoutSurface = impl->_layoutSurface;
    if (!layoutSurface) {
      if(0)printf("  impl has no layoutSurface\n");
      continue;
    }

    fvec2 surfaceUV;
    fvec3 worldHitPos;

    bool hit = impl->rayIntersect(worldRay, camMtx, surfaceUV, worldHitPos);
    if(0)printf("  rayIntersect: hit=%d uv=(%f,%f)\n", hit, surfaceUV.x, surfaceUV.y);

    if (hit) {
      // Hit! Transform UV [0,1] to LayoutSurface pixel coordinates

      int surfaceW = layoutSurface->width();
      int surfaceH = layoutSurface->height();

      // Create transformed event with surface-local coordinates
      auto transformedEv = std::make_shared<Event>();
      *transformedEv = *ev;
      transformedEv->miX = int(surfaceUV.x * surfaceW);
      transformedEv->miY = int(surfaceUV.y * surfaceH);

      if(0)printf("  routing to surface at (%d,%d)\n", transformedEv->miX, transformedEv->miY);

      // On transition from viewport to UI surface, send MOUSE_LEAVE to camera
      if (!_overUiSurface && _camera_evhandler) {
        auto leaveEv = std::make_shared<Event>(*ev);
        leaveEv->mFilteredEvent._eventcode = EventCode::MOUSE_LEAVE;
        _camera_evhandler(leaveEv);
      }
      _overUiSurface = true;

      // Route through the LayoutSurface's widget tree
      auto result = layoutSurface->handleUiEvent(transformedEv);
      if(0)printf("  handleUiEvent returned: handled=%d\n", result.wasHandled());

      // Always block camera events when ray hits the surface,
      // even if no widget handled the event
      HandlerResult hitResult;
      hitResult.setHandled(layoutSurface.get());
      return hitResult;
    }
  }

  // No UI surface hit - clear the flag
  _overUiSurface = false;
  return HandlerResult();
}

///////////////////////////////////////////////////////////////////////////////

HandlerResult SceneGraphViewport::DoOnUiEvent(event_constptr_t ev) {
  //printf("SceneGraphViewport::DoOnUiEvent called\n");
  // 1. Try embedded UI surfaces first
  auto result = _routeToEmbeddedUiSurfaces(ev);
  if (result.wasHandled()) {
    //printf("SceneGraphViewport::DoOnUiEvent 1\n");
    return result;
  }

  // 2. Manipulation handler (e.g., ManipController)
  if (_manip_evhandler && _manipController) {
    // Update ManipController with current camera before handling events
    if (_scenegraph && _scenegraph->_cameralut) {
      auto camera = _scenegraph->_cameralut->find(_cameraname);
      if (camera) {
        float aspect = (height() > 0) ? float(width()) / float(height()) : 1.0f;
        auto camMtx = camera->computeMatrices(aspect);
        _manipController->updateCamera(camMtx, fvec2(width(), height()));
      }
    }
    // Transform event coordinates to viewport-local space
    auto localEv = std::make_shared<Event>(*ev);
    int localX, localY;
    RootToLocal(ev->miX, ev->miY, localX, localY);
    localEv->miX = localX;
    localEv->miY = localY;
    result = _manip_evhandler(localEv);
    if (result.wasHandled()) {
      //printf("SceneGraphViewport::DoOnUiEvent manip\n");
      return result;
    }
  }

  // 3. Camera handler (e.g., EzUiCam)
  if (_camera_evhandler) {
    result = _camera_evhandler(ev);
    if (result.wasHandled()) {
      //printf("SceneGraphViewport::DoOnUiEvent camera\n");
      return result;
    }
  }

  // 4. Default: not handled
  //printf("SceneGraphViewport::DoOnUiEvent not handled\n");
  return HandlerResult();
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
