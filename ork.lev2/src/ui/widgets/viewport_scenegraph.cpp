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
    : Viewport(name, x, y, w, h, fvec4(1,0,1,1), 1.0f) {

} 

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::_doGpuInit(lev2::Context* context) {
  Viewport::_doGpuInit(context);
  _outputnode = std::make_shared<lev2::RtGroupOutputCompositingNode>(_rtgroup);
  _rtgroup->_name = FormatString("ui::SceneGraphViewport<%p>", (void*) this);
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::forkDB(){

  _override_acqdbuf = std::make_shared<lev2::AcquiredDrawQueueForRendering>();
}

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::DoRePaintSurface(ui::drawevent_constptr_t drwev) {
  
  if(_scenegraph){

    _outputnode->setSuperSample(_scenegraph->_SSAA);
    ////////////////////////////////////////////////////
    // in this case we already have a AcquiredDrawQueueForRendering!
    //  provided by ezapp_topwidget enableUiDraw()
    ////////////////////////////////////////////////////

    auto acqbuf = drwev->_acqdbuf;

    if(_override_acqdbuf){
      auto DB = _scenegraph->_dbufcontext_SG->acquireForReadLocked();
      auto WDB = (lev2::DrawQueue*) DB;
      WDB->setUserProperty("vpID"_crcu,_userID);
      auto RCFD = drwev->_acqdbuf->_RCFD;
      _override_acqdbuf->_RCFD = RCFD;
      _override_acqdbuf->_DB = DB;
      acqbuf = _override_acqdbuf;
    }

    auto cimpl = _scenegraph->_compositorImpl;
    cimpl->_cameraName = _cameraname;
    if(_decouple_from_ui_size){
      cimpl->_compcontext->Resize(_decoupled_width,_decoupled_height);
    }
    else{
      cimpl->_compcontext->Resize(width(),height());
    }
    auto comptek = _scenegraph->_compositorTechnique;

    auto orig_onode = comptek->_outputNode;
    comptek->_renderNode->_bufferKey = (uint64_t) this;
    
    comptek->_outputNode = _outputnode;
    
    _scenegraph->_renderWithAcquiredDrawQueueForRendering(acqbuf);

    comptek->_outputNode = orig_onode;

    ////////////////////////////////////////////////////
    if(_override_acqdbuf){
      _scenegraph->_dbufcontext_SG->releaseFromReadLocked(_override_acqdbuf->_DB);
      _override_acqdbuf->_DB = nullptr;
    }
    ////////////////////////////////////////////////////

  }

}
// Surface::DoDraw()

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
