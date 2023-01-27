////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/reflect/properties/registerX.inl>

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
  void gpuInit(lev2::Context* context) {
    if (_doinit) {
      context->debugPushGroup("VRIMPL::gpuInit");
      int width  = orkidvr::device()->_width * 2 * (_vrnode->supersample() + 1);
      int height = orkidvr::device()->_height * (_vrnode->supersample() + 1);

      _blit2screenmtl.gpuInit(context, "orkshader://solid");
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
      _fxtechnique2x2 = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique3x3 = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique4x4 = _blit2screenmtl.technique("downsample_4x4");
      _fxtechnique5x5 = _blit2screenmtl.technique("downsample_5x5");
      _fxtechnique6x6 = _blit2screenmtl.technique("downsample_6x6");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");

      printf("A: vr width<%d> height<%d>\n", width, height);
      _rtg            = new RtGroup(context, width, height, MsaaSamples::MSAA_1X);
      auto buf        = _rtg->createRenderTarget(EBufferFormat::RGBA8);
      buf->_debugName = "WtfVrRt";

      context->debugPopGroup();

      _msaadownsamplebuffer = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      auto dsbuf        = _msaadownsamplebuffer->createRenderTarget(EBufferFormat::RGBA8);
      dsbuf->_debugName = "MsaaDownsampleBuffer";

      _doinit = false;
    }
  }
  ///////////////////////////////////////
  using controllermap_t = const std::map<int, orkidvr::controllerstate_ptr_t>&;
  void renderPoses(Context* targ, CameraMatrices* camdat, controllermap_t controllers) {
    fmtx4 rx;
    fmtx4 ry;
    fmtx4 rz;
    rx.setRotateX(-PI * 0.5);
    ry.setRotateY(PI * 0.5);
    rz.setRotateZ(PI * 0.5);

    for (auto item : controllers) {

      auto controller = item.second;

      fmtx4 scalemtx;
      scalemtx.setScale(controller->_button1Down ? 0.05 : 0.025);

      fmtx4 controller_worldspace = controller->_world_matrix;

      fmtx4 mmtx = fmtx4::multiply_ltor(scalemtx,rx,ry,rz,controller_worldspace);

      targ->MTXI()->PushMMatrix(mmtx);
      targ->MTXI()->PushVMatrix(camdat->GetVMatrix());
      targ->MTXI()->PushPMatrix(camdat->GetPMatrix());
      targ->PushModColor(fvec4::White());
      {
        if (controller->_button2Down)
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

    bool simrunning = true; //drawdata._properties["simrunning"_crcu].get<bool>();
    bool use_vr     = (orkidvr::device()->_active and simrunning);

    /////////////////////////////////////////////////////////////////////////////
    // get VR camera
    /////////////////////////////////////////////////////////////////////////////

    fmtx4 rootmatrix;
    if (use_vr) {
      auto vrcamprop = RCFD.getUserProperty("vrcam"_crc);
      if (auto as_cam = vrcamprop.tryAs<const CameraData*>()) {
        targ->debugMarker("Vr::gotcamera");
        auto vrcam = as_cam.value();
        auto eye   = vrcam->GetEye();
        auto tgt   = vrcam->GetTarget();
        auto up    = vrcam->GetUp();
        rootmatrix.lookAt(eye, tgt, up);
      } else {
        targ->debugMarker("Vr::nocamera");
      }

      _viewOffsetMatrix = orkidvr::device()->_outputViewOffsetMatrix;
    }

    /////////////////////////////////////////////////////////////////////////////

    if (use_vr)
      orkidvr::device()->gpuUpdate(RCFD);

    ///////////////////////////////////

    auto VRDEV = orkidvr::device();
    int width   = VRDEV->_width * 2 * (_vrnode->supersample() + 1);
    int height  = VRDEV->_height * (_vrnode->supersample() + 1);
     //printf( "B: vr width<%d> height<%d>\n", width, height );

    drawdata._properties["OutputWidth"_crcu].set<int>(width);
    drawdata._properties["OutputHeight"_crcu].set<int>(height);
    bool doing_stereo = (use_vr and VRDEV->_supportsStereo);
    drawdata._properties["StereoEnable"_crcu].set<bool>(doing_stereo);
    if (simrunning)
      drawdata._properties["simcammtx"_crcu].set<const CameraMatrices*>(VRDEV->_centercamera);

    if (use_vr and VRDEV->_supportsStereo) {
      RCFD.setUserProperty("vrroot"_crc,rootmatrix);
      _stereomatrices->_left  = VRDEV->_leftcamera;
      _stereomatrices->_right = VRDEV->_rightcamera;
      _stereomatrices->_mono  = VRDEV->_leftcamera;
      drawdata._properties["StereoMatrices"_crcu].set<const StereoCameraMatrices*>(_stereomatrices);
    }

    _CPD.defaultSetup(drawdata);

    _CPD._stereoCameraMatrices = _stereomatrices;

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
  VrCompositingNode* _vrnode            = nullptr;
  StereoCameraMatrices* _stereomatrices = nullptr;
  PoolString _camname, _layers;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  RtGroup* _rtg                      = nullptr;
  bool _doinit                       = true;
  CameraMatrices* _tmpcameramatrices = nullptr;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderTechnique* _fxtechnique2x2;
  const FxShaderTechnique* _fxtechnique3x3;
  const FxShaderTechnique* _fxtechnique4x4;
  const FxShaderTechnique* _fxtechnique5x5;
  const FxShaderTechnique* _fxtechnique6x6;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  rtgroup_ptr_t _msaadownsamplebuffer;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode()
    : _supersample(0) {
  _impl = std::make_shared<VRIMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<VRIMPL>>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::beginAssemble(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("VrCompositingNode::beginAssemble");
  _impl.get<std::shared_ptr<VRIMPL>>()->beginAssemble(drawdata);
  drawdata.context()->debugPopGroup();
}
void VrCompositingNode::endAssemble(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("VrCompositingNode::endAssemble");
  _impl.get<std::shared_ptr<VRIMPL>>()->endAssemble(drawdata);
  drawdata.context()->debugPopGroup();
}

void VrCompositingNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("VrCompositingNode::composite");
  auto impl = _impl.get<std::shared_ptr<VRIMPL>>();
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer      = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = framerenderer.framedata();
  Context* context                     = framedata.GetTarget();
  auto fbi = context->FBI();

  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {

        const auto& vrdev = orkidvr::device();
        auto inp_rtg = drawdata._properties["render_outgroup"_crcu].get<rtgroup_ptr_t>();

        int num_msaa_samples = msaaEnumToInt(tex->_msaa_samples);
        if(num_msaa_samples==1){
          OrkAssert(false);
        }
        else{
          fbi->msaaBlit(inp_rtg,impl->_msaadownsamplebuffer);
          auto this_buf = fbi->GetThisBuffer();
          auto& mtl     = impl->_blit2screenmtl;
          mtl.begin(impl->_fxtechnique1x1, framedata);
          mtl._rasterstate.SetBlending(Blending::OFF);
          tex = impl->_msaadownsamplebuffer->GetMrt(0)->texture();
          mtl.bindParamCTex(impl->_fxpColorMap, tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          ViewportRect extents(0, 0, context->mainSurfaceWidth(), context->mainSurfaceHeight());
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);

        }

        drawdata.context()->debugPushGroup("VrCompositingNode::to_hmd");
        fbi->PushRtGroup(impl->_rtg);
        vrdev->__composite(context, tex);
        fbi->PopRtGroup();
        drawdata.context()->debugPopGroup();
        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("VrCompositingNode::to_screen");

        if (_distorion_lambda) {
          _distorion_lambda(framedata, tex);
        } else {
          /*auto& mtl = impl->_blit2screenmtl;
          mtl.SetAuxMatrix(fmtx4::Identity());
          mtl.SetTexture(tex);
          mtl.SetTexture2(nullptr);
          mtl.SetColorMode(GfxMaterial3DSolid::EMODE_USER);
          mtl._rasterstate.SetBlending(Blending::OFF);
          auto this_buf = fbi->GetThisBuffer();
          int iw        = context->mainSurfaceWidth();
          int ih        = context->mainSurfaceHeight();
          SRect vprect(0, 0, iw, ih);
          fvec4 color(1.0f, 1.0f, 1.0f, 1.0f);
          SRect quadrect(0, ih, iw, 0);
          this_buf->RenderMatOrthoQuad(
              vprect,
              quadrect,
              &mtl,
              0.0f,
              0.0f, // u0 v0
              1.0f,
              1.0f, // u1 v1
              nullptr,
              color);*/
        }

        drawdata.context()->debugPopGroup();
      }
    }
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
