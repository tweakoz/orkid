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
#include <ork/lev2/gfx/pri.h>
#if defined(ENABLE_GLFW)
#include <ork/lev2/glfw/ctx_glfw.h>
#endif

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
    // Render virtual cursor if in fullscreen mouse mode
    ////////////////////////////////////////////////////
#if defined(ENABLE_GLFW)
    auto tgt = drwev->GetTarget();
    auto ctxbase = dynamic_cast<lev2::CtxGLFW*>(tgt->mCtxBase);
    if (ctxbase && ctxbase->_fsMouseMode) {
      auto uiev = ctxbase->uievent();
      int cursorX = uiev->miX;
      // Flip Y: GLFW reports (0,0) at top-left, UI matrix has (0,0) at bottom-left
      int cursorY = height() - uiev->miY;

      // Render a simple crosshair cursor
      auto fbi = tgt->FBI();
      auto mtxi = tgt->MTXI();
      auto primi = tgt->PRI();
      auto defmtl = lev2::defaultUIMaterial();

      int w = width();
      int h = height();
      int cursorSize = 4;
      int cursorThickness = 2;

      fbi->pushViewport(0, 0, w, h);
      fbi->pushScissor(0, 0, w, h);
      auto uimtx = mtxi->uiMatrix(w, h);
      mtxi->PushMMatrix(fmtx4::Identity());
      mtxi->PushVMatrix(fmtx4::Identity());
      mtxi->PushPMatrix(uimtx);
      {
        defmtl->_rasterstate->setBlendingMacro(lev2::BlendingMacro::ALPHA);
        defmtl->_rasterstate->setDepthTest(lev2::EDepthTest::OFF);
        tgt->PushModColor(fvec4(1, 1, 1, 0.9f));
        defmtl->SetUIColorMode(lev2::UiColorMode::MOD);

        // Horizontal bar
        primi->RenderQuadAtZ(
            defmtl.get(),
            cursorX - cursorSize, cursorX + cursorSize,  // x0, x1
            cursorY - cursorThickness/2, cursorY + cursorThickness/2,  // y0, y1
            0.0f,
            0.0f, 1.0f,
            0.0f, 1.0f
        );
        // Vertical bar
        primi->RenderQuadAtZ(
            defmtl.get(),
            cursorX - cursorThickness/2, cursorX + cursorThickness/2,  // x0, x1
            cursorY - cursorSize, cursorY + cursorSize,  // y0, y1
            0.0f,
            0.0f, 1.0f,
            0.0f, 1.0f
        );

        tgt->PopModColor();
      }
      mtxi->PopPMatrix();
      mtxi->PopVMatrix();
      mtxi->PopMMatrix();
      fbi->popScissor();
      fbi->popViewport();
    }
#endif
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
