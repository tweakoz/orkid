////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
    : Viewport(name, x, y, w, h, fvec4(1,0,0,1), 1.0f) {

} 

///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::_doGpuInit(lev2::Context* context) {
  Viewport::_doGpuInit(context);
  _outputnode = std::make_shared<lev2::RtGroupOutputCompositingNode>(_rtgroup);
  _rtgroup->_name = FormatString("ui::SceneGraphViewport<%p>", (void*) this);
}


///////////////////////////////////////////////////////////////////////////////

void SceneGraphViewport::DoRePaintSurface(ui::drawevent_constptr_t drwev) {

  if(_scenegraph){

    ////////////////////////////////////////////////////
    // in this case we already have a AcquiredRenderDrawBuffer!
    //  provided by ezapp_topwidget enableUiDraw()
    ////////////////////////////////////////////////////

    auto acqbuf = drwev->_acqdbuf;
    auto DB = acqbuf->_DB;
    auto rcfd = acqbuf->_RCFD;

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
    _scenegraph->_renderWithAcquiredRenderDrawBuffer(acqbuf);

    comptek->_outputNode = orig_onode;

    ////////////////////////////////////////////////////

  }

}
// Surface::DoDraw()

/////////////////////////////////////////////////////////////////////////

}} // namespace ork::ui
