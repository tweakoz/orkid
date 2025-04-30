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

ImplementReflectionX(ork::lev2::DualMonoVrOutputNode, "DualMonoVrOutputNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::describeX(class_t* c) {
}
///////////////////////////////////////////////////////////////////////////////
struct VRIMPL {
  ///////////////////////////////////////
  VRIMPL(DualMonoVrOutputNode* node)
      : _vrnode(node) {
    _tmpcameramatrices = std::make_shared<CameraMatrices>();
    _stereomatrices    = std::make_shared<StereoCameraMatrices>();
  }
  ///////////////////////////////////////
  ~VRIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* context) {
    if (_doinit) {
      context->debugPushGroup("VRIMPL::gpuInit");
      int width  = orkidvr::device()->_width * 2 * (_vrnode->supersample() + 1);
      int height = orkidvr::device()->_height * (_vrnode->supersample() + 1);

      _blit2screenmtl.gpuInit(context, "orkshader://solid");
      _blit2screenmtl._rasterstate->setCullTest(ECullTest::OFF);
      _fxtechnique_downsample[0] = _blit2screenmtl.technique("texcolor");
      _fxtechnique_downsample[1] = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique_downsample[2] = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique_downsample[3] = _blit2screenmtl.technique("downsample_4x4");
      _fxpMVP                    = _blit2screenmtl.param("MatMVP");
      _fxpColorMap               = _blit2screenmtl.param("ColorMap");
      _ssaadownsamplebuffer      = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      auto dsbuf                 = _ssaadownsamplebuffer->createRenderTarget(_vrnode->_format);
      dsbuf->_debugName          = "MsaaDownsampleBuffer";

      // printf("A: vr width<%d> height<%d>\n", width, height);
      _rtg            = new RtGroup(context, width, height, MsaaSamples::MSAA_1X);
      auto buf        = _rtg->createRenderTarget(EBufferFormat::RGBA8);
      buf->_debugName = "WtfVrRt";

      context->debugPopGroup();

      _doinit = false;
    }
  }
  ///////////////////////////////////////
  void beginAssemble(CompositorDrawData& drawdata) {
    EASY_BLOCK("onodevr-begass");
    auto& ddprops = drawdata._properties;
    auto RCFD     = drawdata.RCFD();
    auto CIMPL    = drawdata._cimpl;
    auto DB       = RCFD->GetDB();
    Context* targ = drawdata.context();

    bool use_vr = (orkidvr::device()->_active);

    auto VRDEV = orkidvr::device();
    int ssaa   = _vrnode->supersample();
    OrkAssert(ssaa >= 0 and ssaa <= 3);
    _multiplier  = ssaa + 1;
    _out_width   = VRDEV->_width * 2;
    _out_height  = VRDEV->_height;
    _ssaa_width  = _out_width * _multiplier;
    _ssaa_height = _out_height * _multiplier;
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

    if (use_vr) {
      orkidvr::device()->gpuUpdate(*RCFD);
    }

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
      drawdata._properties["StereoMatrices"_crcu].set<const StereoCameraMatrices*>(_stereomatrices.get());
    }

    _CPD.defaultSetup(drawdata);

    _CPD._stereoCameraMatrices = _stereomatrices.get();

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
  std::shared_ptr<StereoCameraMatrices> _stereomatrices;
  std::shared_ptr<CameraMatrices> _tmpcameramatrices;

  DualMonoVrOutputNode* _vrnode = nullptr;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  RtGroup* _rtg = nullptr;
  bool _doinit  = true;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique_downsample[4];
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  int _multiplier  = 1;
  int _ssaa_width  = 0;
  int _ssaa_height = 0;
  int _out_width   = 0;
  int _out_height  = 0;
  rtgroup_ptr_t _ssaadownsamplebuffer;
};
///////////////////////////////////////////////////////////////////////////////
DualMonoVrOutputNode::DualMonoVrOutputNode() {
  _impl = std::make_shared<VRIMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
DualMonoVrOutputNode::~DualMonoVrOutputNode() {
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<std::shared_ptr<VRIMPL>>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::beginAssemble(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("DualMonoVrOutputNode::beginAssemble");
  _impl.get<std::shared_ptr<VRIMPL>>()->beginAssemble(drawdata);
  drawdata.context()->debugPopGroup();
}
void DualMonoVrOutputNode::endAssemble(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("DualMonoVrOutputNode::endAssemble");
  _impl.get<std::shared_ptr<VRIMPL>>()->endAssemble(drawdata);
  drawdata.context()->debugPopGroup();
}

void DualMonoVrOutputNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("DualMonoVrOutputNode::composite");
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
        drawdata.context()->debugPushGroup("DualMonoVrOutputNode::to_screen");

        /////////////////////////////////////////////////////////////////////////////
        // be nice and composite to main screen as well...
        /////////////////////////////////////////////////////////////////////////////

        // int num_ssaa_samples = ssaaEnumToInt(tex->_ssaa_samples);

        if (impl->_multiplier != 1) {

          // resize ssaadownsamplebuffer
          auto downRTG = impl->_ssaadownsamplebuffer;
          if (downRTG->width() != impl->_out_width || downRTG->height() != impl->_out_height) {
            downRTG->Resize(impl->_out_width, impl->_out_height);
          }

          fbi->PushRtGroup(downRTG.get());

          auto& mtl = impl->_blit2screenmtl;
          mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
          mtl._rasterstate->setDepthTest(EDepthTest::OFF);
          mtl._rasterstate->setCullTest(ECullTest::OFF);
          mtl._rasterstate->_force = true;

          int ssaa = supersample();
          OrkAssert(ssaa >= 0 and ssaa <= 3);

          auto tek = impl->_fxtechnique_downsample[ssaa];
          drawdata.context()->debugPushGroup("ScreenCompositingNode::to_screen<%d>", ssaa);

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
          drawdata.context()->debugPushGroup("DualMonoVrOutputNode::distortion_lambda");
          _distorion_lambda(framedata, tex);
          drawdata.context()->debugPopGroup();
        } else {
          drawdata.context()->debugPushGroup("DualMonoVrOutputNode::to_hmd");
          const auto& vrdev = orkidvr::device();
          auto& mtl         = impl->_blit2screenmtl;
          auto inp_rtg      = drawdata._properties["final_outgroup"_crcu].get<rtgroup_ptr_t>();
          auto this_buf     = context->FBI()->GetThisBuffer();
          // fbi->PushRtGroup(nullptr);//impl->_rtg);
          // vrdev->__composite(context, tex);
          mtl.begin(impl->_fxtechnique_downsample[0], framedata);

          mtl.bindParamCTex(impl->_fxpColorMap, tex);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());
          ViewportRect extents(0, 0, context->mainSurfaceWidth(), context->mainSurfaceHeight());
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);
          if (_flipY) {
            this_buf->Render2dQuadEML(
                fvec4(-1, -1, 2, 2), // xywh
                fvec4(0, 0, 1, 1),   // uvrectA
                fvec4(0, 0, 1, 1));  // uvrectB
          } else {
            this_buf->Render2dQuadEML(
                fvec4(-1, -1, 2, 2), // xywh
                fvec4(0, 1, 1, -1),  // uvrectA
                fvec4(0, 1, 1, -1)); // uvrectB
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
assembler_fn_t DualMonoVrOutputNode::createAssembler(nodecompositortechnique_ptr_t tek) {
  return [this, tek](CompositorDrawData& drawdata) {
    auto rnode = tek->_renderNode;
    printf("rendering dual-mono-vr!\n");
    ////////////////////////////////////////////////////////////////////////////
    // this assembler will run the render and postfx nodes twice,
    //  once for each eye
    ////////////////////////////////////////////////////////////////////////////
    // for now, so we dont have to change the render and postfx nodes
    //  we will stash each eye in a holding buffer for final compositing
    ////////////////////////////////////////////////////////////////////////////
    auto do_for_eye = [&](bool is_left_eye) {
      ////////////////////////////////////////////////////////////////////////////
      bool is_right_eye = not is_left_eye;
      ////////////////////////////////////////////////////////////////////////////
      rtgroup_ptr_t render_outg = rnode ? rnode->GetOutputGroup() : nullptr;
      RtBuffer* render_out      = rnode ? rnode->GetOutput().get() : nullptr;
      drawdata._properties["render_out"_crcu].set<RtBuffer*>(render_out);
      drawdata._properties["render_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
      ////////////////////////////////////////////////////////////////////////////
      this->beginAssemble(drawdata);
      rnode->Render(drawdata);
      this->endAssemble(drawdata);
      size_t num_fx_nodes = tek->_postEffectNodes.size();
      for (auto pfxnode : tek->_postEffectNodes) {
        drawdata._properties["postfx_in"_crcu].set<rtgroup_ptr_t>(render_outg);
        pfxnode->Render(drawdata);
        render_outg = pfxnode->GetOutputGroup();
        render_out  = pfxnode->GetOutput().get();
      }
      drawdata._properties["final_out"_crcu].set<RtBuffer*>(render_out);
      drawdata._properties["final_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
    };
    do_for_eye(false);
    // do_for_eye(true);
  };
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
