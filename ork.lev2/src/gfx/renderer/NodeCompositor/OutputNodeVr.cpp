////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/vr/vr.h>
#include <ork/profiling.inl>

ImplementReflectionX(ork::lev2::VrCompositingNode, "VrCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
struct VRIMPL {
  ///////////////////////////////////////
  VRIMPL(VrCompositingNode* node)
      : _vrnode(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {

    _tmpcameramatrices = new CameraMatrices;
    _stereomatrices    = new StereoCameraMatrices;
  }
  ///////////////////////////////////////
  ~VRIMPL() {
    delete _tmpcameramatrices;
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* pTARG) {
    if (_doinit) {
      pTARG->debugPushGroup("VRIMPL::gpuInit");
      int width  = orkidvr::device()._width * 2;
      int height = orkidvr::device()._height;
      _blit2screenmtl.SetUserFx("orkshader://solid", "texcolor");
      _blit2screenmtl.gpuInit(pTARG);

      _rtg            = new RtGroup(pTARG, width, height, 1);
      auto buf        = new RtBuffer(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA8, width, height);
      buf->_debugName = "WtfVrRt";
      _rtg->SetMrt(0, buf);

      pTARG->debugPopGroup();

      _doinit = false;
    }
  }
  ///////////////////////////////////////
  typedef const std::map<int, orkidvr::ControllerState>& controllermap_t;
  void renderPoses(Context* targ, CameraMatrices* camdat, controllermap_t controllers) {
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
      scalemtx.SetScale(c._button1Down ? 0.05 : 0.025);

      fmtx4 controller_worldspace = (c._matrix * ivomatrix);

      fmtx4 mmtx = (scalemtx * rx * ry * rz * controller_worldspace);

      targ->MTXI()->PushMMatrix(mmtx);
      targ->MTXI()->PushVMatrix(camdat->GetVMatrix());
      targ->MTXI()->PushPMatrix(camdat->GetPMatrix());
      targ->PushModColor(fvec4::White());
      {
        if (c._button2Down)
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
    EASY_BLOCK("onodevr-begass");
    auto& ddprops                = drawdata._properties;
    FrameRenderer& framerenderer = drawdata.mFrameRenderer;
    RenderContextFrameData& RCFD = framerenderer.framedata();
    auto CIMPL                   = drawdata._cimpl;
    auto DB                      = RCFD.GetDB();
    Context* targ                = drawdata.context();

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

    if (use_vr)
      orkidvr::gpuUpdate(RCFD);

    ///////////////////////////////////

    auto& VRDEV = orkidvr::device();
    int width   = VRDEV._width * 2;
    int height  = VRDEV._height;

    drawdata._properties["OutputWidth"_crcu].Set<int>(width);
    drawdata._properties["OutputHeight"_crcu].Set<int>(height);
    bool doing_stereo = (use_vr and VRDEV._supportsStereo);
    drawdata._properties["StereoEnable"_crcu].Set<bool>(doing_stereo);
    if (simrunning)
      drawdata._properties["simcammtx"_crcu].Set<const CameraMatrices*>(VRDEV._centercamera);

    if (use_vr and VRDEV._supportsStereo) {
      _stereomatrices->_left  = VRDEV._leftcamera;
      _stereomatrices->_right = VRDEV._rightcamera;
      _stereomatrices->_mono  = VRDEV._leftcamera;
      drawdata._properties["StereoMatrices"_crcu].Set<const StereoCameraMatrices*>(_stereomatrices);
    }

    _CPD.defaultSetup(drawdata);

    //////////////////////////////////////////////////////

    CIMPL->pushCPD(_CPD);
  }
  ///////////////////////////////////////
  void endAssemble(CompositorDrawData& drawdata) {
    EASY_BLOCK("onodevr-endass");
    auto CIMPL                   = drawdata._cimpl;
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
  RtGroup* _rtg                      = nullptr;
  bool _doinit                       = true;
  CameraMatrices* _tmpcameramatrices = nullptr;
  ork::lev2::GfxMaterial3DSolid _blit2screenmtl;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode() {
  _impl = std::make_shared<VRIMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.Get<std::shared_ptr<VRIMPL>>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("VrCompositingNode::beginAssemble");
  _impl.Get<std::shared_ptr<VRIMPL>>()->beginAssemble(drawdata);
  drawdata.context()->debugPopGroup();
}
void VrCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("VrCompositingNode::endAssemble");
  _impl.Get<std::shared_ptr<VRIMPL>>()->endAssemble(drawdata);
  drawdata.context()->debugPopGroup();
}

void VrCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("VrCompositingNode::composite");
  auto impl = _impl.Get<std::shared_ptr<VRIMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer      = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = framerenderer.framedata();
  Context* targ                     = framedata.GetTarget();
  if (auto try_final = drawdata._properties["final_out"_crcu].TryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {
        drawdata.context()->debugPushGroup("VrCompositingNode::to_hmd");
        targ->FBI()->PushRtGroup(impl->_rtg);
        orkidvr::composite(targ, tex);
        targ->FBI()->PopRtGroup();
        drawdata.context()->debugPopGroup();
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("VrCompositingNode::to_screen");
        auto this_buf = targ->FBI()->GetThisBuffer();
        auto& mtl     = impl->_blit2screenmtl;
        int iw        = targ->mainSurfaceWidth();
        int ih        = targ->mainSurfaceHeight();
        SRect vprect(0, 0, iw, ih);
        SRect quadrect(0, ih, iw, 0);
        fvec4 color(1.0f, 1.0f, 1.0f, 1.0f);
        mtl.SetAuxMatrix(fmtx4::Identity());
        mtl.SetTexture(tex);
        mtl.SetTexture2(nullptr);
        mtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
        mtl._rasterstate.SetBlending(EBLENDING_OFF);
        this_buf->RenderMatOrthoQuad(
            vprect,
            quadrect,
            &mtl,
            0.0f,
            0.0f, // u0 v0
            1.0f,
            1.0f, // u1 v1
            nullptr,
            color);
        drawdata.context()->debugPopGroup();
      }
    }
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
