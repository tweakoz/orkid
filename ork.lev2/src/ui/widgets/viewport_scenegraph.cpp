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

INSTANTIATE_TRANSPARENT_RTTI(ork::ui::SceneGraphViewport, "ui::SceneGraphViewport");

namespace ork { namespace ui {

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

SceneGraphViewport::SceneGraphViewport(const std::string& name, int x, int y, int w, int h)
    : Viewport(name, x, y, w, h, fvec4(1, 0, 1, 1), 1.0f) {
  _flipY = false;
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

HandlerResult SceneGraphViewport::DoOnUiEvent(event_constptr_t ev) {
  // First, try to route to any registered UI surfaces
  if (_scenegraph) {
    const auto& uiSurfaces = _scenegraph->uiSurfaces();
    if (!uiSurfaces.empty()) {
      // Get camera matrices from the scenegraph's camera
      auto cameralut = _scenegraph->_cameralut;
      if (cameralut) {
        auto camera = cameralut->find(_cameraname);
        if (camera) {
          float aspect = (height() > 0) ? float(width()) / float(height()) : 1.0f;
          auto camMtx = camera->computeMatrices(aspect);

          // Check each UI surface for hit
          for (auto& drawable : uiSurfaces) {
            auto impl = lev2::getUISurfaceRenderImpl(drawable);
            if (impl) {
              auto result = impl->routeUiEvent(width(), height(), camMtx, ev);
              if (result.mHandler != nullptr) {
                return result;  // Event was consumed by UI surface
              }
            }
          }
        }
      }
    }
  }

  // No UI surface consumed the event - return empty result
  // The widget's _evhandler will be called by the base class
  return HandlerResult();
}

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
