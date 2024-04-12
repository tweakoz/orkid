////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
      : _vrnode(node) {

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
      _blit2screenmtl._rasterstate.SetCullTest(ECullTest::OFF);
      _fxtechnique1x1 = _blit2screenmtl.technique("texcolor");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");

      // printf("A: vr width<%d> height<%d>\n", width, height);
      _rtg            = new RtGroup(context, width, height, MsaaSamples::MSAA_1X);
      auto buf        = _rtg->createRenderTarget(EBufferFormat::RGBA8);
      buf->_debugName = "WtfVrRt";

      context->debugPopGroup();

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

      fmtx4 mmtx = fmtx4::multiply_ltor(scalemtx, rx, ry, rz, controller_worldspace);

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

    bool use_vr = (orkidvr::device()->_active);

    auto VRDEV = orkidvr::device();

    int width    = VRDEV->_width * 2 * (_vrnode->supersample() + 1);
    int height   = VRDEV->_height * (_vrnode->supersample() + 1);
    float aspect = float(width) / float(height);
    /////////////////////////////////////////////////////////////////////////////
    // get VR camera
    /////////////////////////////////////////////////////////////////////////////

    fmtx4 rootmatrix;
    if (use_vr) {
      // printf( "WTF active\n");
      auto vrdev_camname = VRDEV->_cameraName;
      if (vrdev_camname != "") {
        cameradata_constptr_t camera;
        DB->_cameraDataLUT.atomicOp([&](const cameradatalut_ptr_t& unlocked) { camera = unlocked->find(vrdev_camname); });
        if (camera) {
          auto camdat           = camera.get();
          (*_tmpcameramatrices) = camdat->computeMatrices(aspect);
          rootmatrix            = _tmpcameramatrices->GetVMatrix();
          // rootmatrix.dump("yo");
        }
      } else {
        auto vrcamprop = RCFD.getUserProperty("vrcam"_crc);
        if (auto as_cam = vrcamprop.tryAs<const CameraData*>()) {
          targ->debugMarker("Vr::gotcamera");
          auto vrcam = as_cam.value();
          auto eye   = vrcam->GetEye();
          auto tgt   = vrcam->GetTarget();
          auto up    = vrcam->GetUp();
          rootmatrix.lookAt(eye, tgt, up);
          rootmatrix.dump("yo");
        } else {
          targ->debugMarker("Vr::nocamera");
          // printf("novrcam\n");
        }
      }

      _viewOffsetMatrix = orkidvr::device()->_outputViewOffsetMatrix;
    }

    /////////////////////////////////////////////////////////////////////////////

    if (use_vr)
      orkidvr::device()->gpuUpdate(RCFD);

    ///////////////////////////////////

    // printf( "B: vr width<%d> height<%d>\n", width, height );

    drawdata._properties["OutputWidth"_crcu].set<int>(width);
    drawdata._properties["OutputHeight"_crcu].set<int>(height);
    bool doing_stereo = (use_vr and VRDEV->_supportsStereo);
    drawdata._properties["StereoEnable"_crcu].set<bool>(doing_stereo);
    drawdata._properties["simcammtx"_crcu].set<const CameraMatrices*>(VRDEV->_centercamera);

    if (use_vr and VRDEV->_supportsStereo) {
      RCFD.setUserProperty("vrroot"_crc, rootmatrix);
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
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  RtGroup* _rtg                      = nullptr;
  bool _doinit                       = true;
  CameraMatrices* _tmpcameramatrices = nullptr;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique1x1;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
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
  Context* context                  = framedata.GetTarget();
  auto fbi                          = context->FBI();

  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {

        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("VrCompositingNode::to_screen");

        if (_distorion_lambda) {
          _distorion_lambda(framedata, tex);
        } else {
          drawdata.context()->debugPushGroup("VrCompositingNode::to_hmd");
          const auto& vrdev = orkidvr::device();
          auto& mtl         = impl->_blit2screenmtl;
          auto inp_rtg      = drawdata._properties["render_outgroup"_crcu].get<rtgroup_ptr_t>();
          auto this_buf     = context->FBI()->GetThisBuffer();
          // fbi->PushRtGroup(nullptr);//impl->_rtg);
          // vrdev->__composite(context, tex);
          mtl.begin(impl->_fxtechnique1x1, framedata);

          mtl._rasterstate.SetBlending(Blending::OFF);
          mtl.bindParamCTex(impl->_fxpColorMap, tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          ViewportRect extents(0, 0, context->mainSurfaceWidth(), context->mainSurfaceHeight());
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);

          // fbi->PopRtGroup();
          drawdata.context()->debugPopGroup();
        }

        drawdata.context()->debugPopGroup();
      }
    }
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
