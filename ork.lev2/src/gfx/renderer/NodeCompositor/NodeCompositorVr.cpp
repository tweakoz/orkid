////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorVr.h"
#include <ork/application/application.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/vr/vr.h>

ImplementReflectionX(ork::lev2::VrCompositingNode, "VrCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::describeX(class_t* c) {}
///////////////////////////////////////////////////////////////////////////////
struct VRIMPL {
  ///////////////////////////////////////
  VRIMPL(VrCompositingNode* node)
      : _vrnode(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {

    _tmpcameramatrices = new CameraMatrices;
    _stereomatrices = new StereoCameraMatrices;
  }
  ///////////////////////////////////////
  ~VRIMPL() { delete _tmpcameramatrices; }
  ///////////////////////////////////////
  void gpuInit(lev2::GfxTarget* pTARG) {
    if (_doinit) {
      pTARG->debugPushGroup("VRIMPL::gpuInit");
      _width  = orkidvr::device()._width * 2;
      _height = orkidvr::device()._height;
      _blit2screenmtl.SetUserFx("orkshader://solid", "texcolor");
      _blit2screenmtl.Init(pTARG);

      _rtg            = new RtGroup(pTARG, _width, _height, 1);
      auto buf        = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, _width, _height);
      buf->_debugName = "WtfVrRt";
      _rtg->SetMrt(0, buf);

      pTARG->debugPopGroup();

      _doinit = false;
    }
  }
  ///////////////////////////////////////
  typedef const std::map<int, orkidvr::ControllerState>& controllermap_t;
  void renderPoses(GfxTarget* targ, CameraMatrices* camdat, controllermap_t controllers) {
    fmtx4 rx;
    fmtx4 ry;
    fmtx4 rz;
    rx.SetRotateX(-PI * 0.5);
    ry.SetRotateY(PI * 0.5);
    rz.SetRotateZ(PI * 0.5);

    for (auto item : controllers) {

      auto c = item.second;
      fmtx4 ivomatrix;
      ivomatrix.inverseOf(_viewOffsetMatrix);

      fmtx4 scalemtx;
      scalemtx.SetScale(c._button1down ? 0.05 : 0.025);

      fmtx4 controller_worldspace = (c._matrix * ivomatrix);

      fmtx4 mmtx = (scalemtx * rx * ry * rz * controller_worldspace);

      targ->MTXI()->PushMMatrix(mmtx);
      targ->MTXI()->PushVMatrix(camdat->GetVMatrix());
      targ->MTXI()->PushPMatrix(camdat->GetPMatrix());
      targ->PushModColor(fvec4::White());
      {
        if (c._button2down)
          ork::lev2::GfxPrimitives::GetRef().RenderBox(targ);
        else
          ork::lev2::GfxPrimitives::GetRef().RenderAxis(targ);
      }
      targ->PopModColor();
      targ->MTXI()->PopPMatrix();
      targ->MTXI()->PopVMatrix();
      targ->MTXI()->PopMMatrix();
    }
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    auto& ddprops                = drawdata._properties;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL = drawdata._cimpl;
    auto DB                      = RCFD.GetDB();
    GfxTarget* targ              = drawdata.target();

    bool simrunning = drawdata._properties["simrunning"_crcu].Get<bool>();
    bool use_vr     = (orkidvr::device()._active and simrunning);

    /////////////////////////////////////////////////////////////////////////////
    // get VR camera
    /////////////////////////////////////////////////////////////////////////////

    if (use_vr) {
      auto vrcamprop = RCFD.getUserProperty("vrcam"_crc);
      fmtx4 rootmatrix;
      if (auto as_cam = vrcamprop.TryAs<const CameraData*>()) {
        targ->debugMarker("Vr::gotcamera");
        auto vrcam = as_cam.value();
        auto eye   = vrcam->GetEye();
        auto tgt   = vrcam->GetTarget();
        auto up    = vrcam->GetUp();
        rootmatrix.LookAt(eye, tgt, up);
      } else {
        targ->debugMarker("Vr::nocamera");
      }

      _viewOffsetMatrix = orkidvr::device()._outputViewOffsetMatrix;
    }

    /////////////////////////////////////////////////////////////////////////////

    _CPD.AddLayer("All"_pool);

    if (use_vr)
      orkidvr::gpuUpdate(RCFD);

    ///////////////////////////////////
    // float w = _rtg->GetW(); float h = _rtg->GetH();
    ///////////////////////////////////

    SRect tgt_rect(0, 0, _width, _height);

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = nullptr;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = nullptr; // default == "All"
    _CPD._clearColor  = fvec4(0.61, 0, 0, 1);

    //////////////////////////////////////////////////////
    // is stereo active
    //////////////////////////////////////////////////////

    auto& VRDEV = orkidvr::device();

    if (use_vr and VRDEV._supportsStereo) {
      _stereomatrices->_left = VRDEV._leftcamera;
      _stereomatrices->_right = VRDEV._rightcamera;
      _stereomatrices->_mono = VRDEV._leftcamera;
      _CPD.setStereoOnePass(true);
      _CPD._stereoCameraMatrices = _stereomatrices;
      _CPD._cameraMatrices = nullptr;
    } else {
      ////////////////////////////////////////////////
      // no stereo cam support, override cam with center side of stereocam
      //  eg. when using mac VR emulation
      ////////////////////////////////////////////////
      _CPD.setStereoOnePass(false);
      _CPD._stereoCameraMatrices = nullptr;
      _CPD._cameraMatrices = simrunning
                           ? VRDEV._centercamera
                           : ddprops["defcammtx"_crcu].Get<const CameraMatrices*>();
      ////////////////////////////////////////////////
    }

    //////////////////////////////////////////////////////

    _CPD.SetDstRect(tgt_rect);
    drawdata._properties["OutputWidth"_crcu].Set<int>(_width);
    drawdata._properties["OutputHeight"_crcu].Set<int>(_height);
    CIMPL->pushCPD(_CPD);
  }
  ///////////////////////////////////////
  void endAssemble(CompositorDrawData& drawdata) {
    auto CIMPL = drawdata._cimpl;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  StereoCameraMatrices* _stereomatrices = nullptr;
  PoolString _camname, _layers;
  VrCompositingNode* _vrnode = nullptr;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  RtGroup* _rtg           = nullptr;
  int _width              = 0;
  int _height             = 0;
  bool _doinit            = true;
  CameraMatrices* _tmpcameramatrices = nullptr;
  ork::lev2::GfxMaterial3DSolid _blit2screenmtl;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode() { _impl = std::make_shared<VRIMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::gpuInit(lev2::GfxTarget* pTARG, int iW, int iH) { _impl.Get<std::shared_ptr<VRIMPL>>()->gpuInit(pTARG); }
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  drawdata.target()->debugPushGroup("VrCompositingNode::beginAssemble");
  _impl.Get<std::shared_ptr<VRIMPL>>()->beginAssemble(drawdata);
  drawdata.target()->debugPopGroup();
}
void VrCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  drawdata.target()->debugPushGroup("VrCompositingNode::endAssemble");
  _impl.Get<std::shared_ptr<VRIMPL>>()->endAssemble(drawdata);
  drawdata.target()->debugPopGroup();
}

void VrCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.target()->debugPushGroup("VrCompositingNode::composite");
  auto impl = _impl.Get<std::shared_ptr<VRIMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer      = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = framerenderer.framedata();
  GfxTarget* targ                   = framedata.GetTarget();
  if (auto try_final = drawdata._properties["final_out"_crcu].TryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->GetTexture();
      if (tex) {
        drawdata.target()->debugPushGroup("VrCompositingNode::to_hmd");
        targ->FBI()->PushRtGroup(impl->_rtg);
        orkidvr::composite(targ, tex);
        targ->FBI()->PopRtGroup();
        drawdata.target()->debugPopGroup();
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.target()->debugPushGroup("VrCompositingNode::to_screen");
        auto this_buf = targ->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        int iw        = targ->GetW();
        int ih        = targ->GetH();
        SRect vprect(0, 0, iw, ih);
        SRect quadrect(0, ih, iw, 0);
        fvec4 color(1.0f, 1.0f, 1.0f, 1.0f);
        mtl.SetAuxMatrix(fmtx4::Identity);
        mtl.SetTexture(tex);
        mtl.SetTexture2(nullptr);
        mtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
        mtl.mRasterState.SetBlending(EBLENDING_OFF);
        this_buf->RenderMatOrthoQuad(vprect,
                                     quadrect,
                                     &mtl,
                                     0.0f,
                                     0.0f, // u0 v0
                                     1.0f,
                                     1.0f, // u1 v1
                                     nullptr,
                                     color);
        drawdata.target()->debugPopGroup();
      }
    }
  }
  drawdata.target()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
