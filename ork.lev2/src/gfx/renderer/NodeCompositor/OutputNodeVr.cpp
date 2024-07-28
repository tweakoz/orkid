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
      _fxtechnique2x2       = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique3x3       = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique4x4       = _blit2screenmtl.technique("downsample_4x4");
      _fxtechnique5x5       = _blit2screenmtl.technique("downsample_5x5");
      _fxtechnique6x6       = _blit2screenmtl.technique("downsample_6x6");
      _fxpMVP         = _blit2screenmtl.param("MatMVP");
      _fxpColorMap    = _blit2screenmtl.param("ColorMap");
      _ssaadownsamplebuffer = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      auto dsbuf            = _ssaadownsamplebuffer->createRenderTarget(_vrnode->_format);
      dsbuf->_debugName     = "MsaaDownsampleBuffer";

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
    auto RCFD = drawdata.RCFD();
    auto CIMPL                   = drawdata._cimpl;
    auto DB                      = RCFD->GetDB();
    Context* targ                = drawdata.context();

    bool use_vr = (orkidvr::device()->_active);

    auto VRDEV = orkidvr::device();
    _multiplier = 1;
    switch (_vrnode->supersample()) {
      case 0:
        _multiplier = 1;
        break;
      case 1:
        _multiplier = 2;
        break;
      case 2:
        _multiplier = 3;
        break;
      case 3:
        _multiplier = 4;
        break;
      case 4:
        _multiplier = 5;
        break;
      case 5:
        _multiplier = 6;
        break;
      default:
        OrkAssert(false);
    }
    _out_width     = VRDEV->_width*2;

    _out_height     = VRDEV->_height;
    _ssaa_width    = _out_width * _multiplier;
    _ssaa_height   = _out_height * _multiplier;
    float aspect = float(_ssaa_width) / float(_ssaa_height);
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
        auto vrcamprop = RCFD->getUserProperty("vrcam"_crc);
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
      orkidvr::device()->gpuUpdate(*RCFD);

    ///////////////////////////////////

    // printf( "B: vr width<%d> height<%d>\n", width, height );

    drawdata._properties["OutputWidth"_crcu].set<int>(_ssaa_width);
    drawdata._properties["OutputHeight"_crcu].set<int>(_ssaa_height);
    bool doing_stereo = (use_vr and VRDEV->_supportsStereo);
    drawdata._properties["StereoEnable"_crcu].set<bool>(doing_stereo);
    drawdata._properties["simcammtx"_crcu].set<const CameraMatrices*>(VRDEV->_centercamera);

    if (use_vr and VRDEV->_supportsStereo) {
      RCFD->setUserProperty("vrroot"_crc, rootmatrix);
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
    auto CIMPL = drawdata._cimpl;
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
  const FxShaderTechnique* _fxtechnique2x2;
  const FxShaderTechnique* _fxtechnique3x3;
  const FxShaderTechnique* _fxtechnique4x4;
  const FxShaderTechnique* _fxtechnique5x5;
  const FxShaderTechnique* _fxtechnique6x6;
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  int _multiplier = 1;
  int _ssaa_width = 0;
  int _ssaa_height = 0;
  int _out_width = 0;
  int _out_height = 0;
  rtgroup_ptr_t _ssaadownsamplebuffer;
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
  Context* context = drawdata.context();
  auto fbi         = context->FBI();
  auto gbi         = context->GBI();

  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto buffer = try_final.value();
    if (buffer) {
      assert(buffer != nullptr);
      auto tex = buffer->texture();
      if (tex) {
        auto framedata = drawdata.RCFD();

        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////
        drawdata.context()->debugPushGroup("VrCompositingNode::to_screen");

                /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////

        //int num_ssaa_samples = ssaaEnumToInt(tex->_ssaa_samples);

        if (impl->_multiplier != 1) {

          // resize ssaadownsamplebuffer
          auto downRTG = impl->_ssaadownsamplebuffer;
          if(downRTG->width()!=impl->_out_width || downRTG->height()!=impl->_out_height) {
            downRTG->Resize(impl->_out_width, impl->_out_height);
          }

          fbi->PushRtGroup(downRTG.get());

          auto& mtl     = impl->_blit2screenmtl;
          switch (this->_supersample) {
            case 0:
              drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<0>");
              mtl.begin(impl->_fxtechnique1x1, framedata);
              break;
            case 1:
              drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<1>");
              mtl.begin(impl->_fxtechnique2x2, framedata);
              break;
            case 2:
              drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<2>");
              mtl.begin(impl->_fxtechnique3x3, framedata);
              break;
            case 3:
              drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<3>");
              mtl.begin(impl->_fxtechnique4x4, framedata);
              break;
            case 4:
              drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<4>");
              mtl.begin(impl->_fxtechnique5x5, framedata);
              break;
            case 5:
              drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<5>");
              mtl.begin(impl->_fxtechnique6x6, framedata);
              break;
            default:
              OrkAssert(false);
              break;
          }

          mtl._rasterstate.SetBlending(Blending::OFF);
          mtl.bindParamCTex(impl->_fxpColorMap, tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          ViewportRect extents(0, 0, impl->_out_width, impl->_out_height);
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          gbi->render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
          fbi->popViewport();
          fbi->popScissor();
          mtl.end(framedata);
          fbi->PopRtGroup();

          tex = downRTG->GetMrt(0)->texture();

          drawdata.context()->debugPopGroup();   

        }

        if (_distorion_lambda) {
          _distorion_lambda(framedata, tex);
        } else {
          drawdata.context()->debugPushGroup("VrCompositingNode::to_hmd");
          const auto& vrdev = orkidvr::device();
          auto& mtl         = impl->_blit2screenmtl;
          auto inp_rtg      = drawdata._properties["final_outgroup"_crcu].get<rtgroup_ptr_t>();
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
          if(_flipY){
            this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 1, 1));
          }
          else{
            this_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 1, 1, -1), fvec4(0, 1, 1, -1));            
          }
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
