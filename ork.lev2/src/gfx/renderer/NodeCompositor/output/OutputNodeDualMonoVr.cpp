////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/application/application.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/pri.h>
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
struct DMVRIMPL {
  ///////////////////////////////////////
  DMVRIMPL(DualMonoVrOutputNode* node)
      : _vrnode(node) {
    _tmpcameramatrices = std::make_shared<CameraMatrices>();
    _stereomatrices    = std::make_shared<StereoCameraMatrices>();
  }
  ///////////////////////////////////////
  ~DMVRIMPL() {
  }
  ///////////////////////////////////////
  void gpuInit(lev2::Context* context) {
    if (_doinit) {
      int width  = orkidvr::device()->_width * 2 * (_vrnode->supersample() + 1);
      int height = orkidvr::device()->_height * (_vrnode->supersample() + 1);

      _blit2screenmtl.gpuInit(context, "orkshader://blit");
      _blit2screenmtl._rasterstate->setCullTest(ECullTest::OFF);
      _fxtechnique_downsample[0] = _blit2screenmtl.technique("blituv");
      _fxtechnique_downsample[1] = _blit2screenmtl.technique("downsample_2x2");
      _fxtechnique_downsample[2] = _blit2screenmtl.technique("downsample_3x3");
      _fxtechnique_downsample[3] = _blit2screenmtl.technique("downsample_4x4");
      _fxpMVP                    = _blit2screenmtl.param("MatMVP");
      _fxpColorMap               = _blit2screenmtl.param("ColorMap");
      _ssaadownsamplebufferL     = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      _ssaadownsamplebufferR     = std::make_shared<RtGroup>(context, 8, 8, MsaaSamples::MSAA_1X);
      _ssaadownsamplebufferL->_name = "dmvr.downsampleL";
      _ssaadownsamplebufferR->_name = "dmvr.downsampleR";
      auto dsbufL                = _ssaadownsamplebufferL->createRenderTarget(_vrnode->_format);
      dsbufL->_debugName         = "MsaaDownsampleBufferL";
      auto dsbufR                = _ssaadownsamplebufferR->createRenderTarget(_vrnode->_format);
      dsbufR->_debugName         = "MsaaDownsampleBufferR";

      _doinit = false;
    }
  }
  ///////////////////////////////////////
  void _beginAssembleEye(CompositorDrawData& drawdata, bool is_left_eye) {
    EASY_BLOCK("onodevr-begass");
    auto& ddprops = drawdata._properties;
    auto RCFD     = drawdata.RCFD();
    auto CIMPL    = drawdata._cimpl;
    auto DB       = RCFD->GetDB();
    Context* targ = drawdata.context();

    auto VRDEV = orkidvr::device();
    int ssaa   = _vrnode->supersample();
    OrkAssert(ssaa >= 0 and ssaa <= 3);
    _multiplier     = ssaa + 1;
    _per_eye_width  = VRDEV->_width; // * 2;
    _per_eye_height = VRDEV->_height;
    _ssaa_width     = _per_eye_width * _multiplier;
    _ssaa_height    = _per_eye_height * _multiplier;
    float aspect    = float(_ssaa_width) / float(_ssaa_height);

    /////////////////////////////////////////////////////////////////////////////
    // get VR camera
    /////////////////////////////////////////////////////////////////////////////

    fmtx4 rootmatrix;
    // printf( "WTF active\n");
    auto vrdev_camname = VRDEV->_camera_name;
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
      if (auto as_cam = RCFD->tryUserProperty<const CameraData*>("vrcam"_crc)) {
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

    /////////////////////////////////////////////////////////////////////////////

    orkidvr::device()->gpuUpdate(*RCFD);

    ///////////////////////////////////

    // printf( "B: vr width<%d> height<%d>\n", width, height );

    drawdata._properties["OutputWidth"_crcu].set<int>(_ssaa_width);
    drawdata._properties["OutputHeight"_crcu].set<int>(_ssaa_height);

    // we are not using single pass stereo,
    //  so we will render using a mono camera
    //  and we will switch the mono camera from left to right
    //  depending on the eye we are rendering

    drawdata._properties["SinglePassStereo"_crcu].set<bool>(false);
    auto mono_cam = is_left_eye ? VRDEV->_leftcamera : VRDEV->_rightcamera;
    drawdata._properties["defcammtx"_crcu].set<cameramatrices_ptr_t>(mono_cam);
    drawdata._properties["centercam"_crcu].set<cameramatrices_ptr_t>(VRDEV->_centercamera);
    mono_cam->_camdat.Persp(VRDEV->_near, VRDEV->_far, VRDEV->_fov*RTOD);
    RCFD->setUserProperty("vrroot"_crc, rootmatrix);
    _stereomatrices->_left  = VRDEV->_leftcamera;
    _stereomatrices->_right = VRDEV->_rightcamera;
    _stereomatrices->_mono  = VRDEV->_centercamera;
    drawdata._properties["StereoMatrices"_crcu].set<const StereoCameraMatrices*>(_stereomatrices.get());
    drawdata._properties["eyeindex"_crcu].set<int>(is_left_eye ? 0 : 1);
    using smat_ptr_t = const StereoCameraMatrices*;
    RCFD->setUserProperty("StereoMatrices"_crcu, (smat_ptr_t) _stereomatrices.get());
    RCFD->setUserProperty("eyeindex"_crcu, int(is_left_eye ? 0 : 1));
    _CPD.defaultSetup(drawdata);

    _CPD._stereo_cam_matrices = _stereomatrices.get();

    //////////////////////////////////////////////////////

    CIMPL->pushCPD(_CPD);

    if (_vrnode->_onCameraChange) {
      _vrnode->_onCameraChange(drawdata);
    }
  }
  ///////////////////////////////////////
  void _endAssembleEye(CompositorDrawData& drawdata, bool is_left_eye) {
    EASY_BLOCK("onodevr-endass");
    auto CIMPL = drawdata._cimpl;
    CIMPL->popCPD();
  }
  ///////////////////////////////////////
  void _downsample(
      CompositorDrawData& drawdata, //
      RtBuffer* render_out,         //
      bool is_left_eye) {           //

    auto context   = drawdata.context();
    auto fbi       = context->FBI();
    auto gbi       = context->GBI();
    auto dwi       = context->DWI();
    auto framedata = drawdata.RCFD();
    auto tex       = render_out->texture();
    auto this_buf  = context->FBI()->GetThisBuffer();
    // resize ssaadownsamplebuffer
    auto downRTG = is_left_eye ? _ssaadownsamplebufferL : _ssaadownsamplebufferR;
    // printf("_per_eye_width<%d> _per_eye_height<%d>\n", _per_eye_width, _per_eye_height);
    if (downRTG->width() != _per_eye_width || downRTG->height() != _per_eye_height) {
      downRTG->Resize(_per_eye_width, _per_eye_height);
    }

    fbi->PushRtGroup(downRTG.get());

    auto& mtl = _blit2screenmtl;
    mtl._rasterstate->setBlendingMacro(BlendingMacro::OFF);
    mtl._rasterstate->setDepthTest(EDepthTest::OFF);
    mtl._rasterstate->setCullTest(ECullTest::OFF);
    mtl._rasterstate->_force = true;

    int ssaa = _vrnode->supersample();
    OrkAssert(ssaa >= 0 and ssaa <= 3);

    auto tek = _fxtechnique_downsample[ssaa];
    if(tek==nullptr){
      printf("WTF: tek is null for ssaa<%d>\n", ssaa);
      OrkAssert(false);
    }
    context->debugPushGroup("ScreenCompositingNode::to_screen<%d>", ssaa);

    mtl.begin(tek, framedata);
    mtl.bindParamTexture(_fxpColorMap, tex);
    mtl.bindParamMatrix(_fxpMVP, fmtx4::Identity());
    ViewportRect extents(0, 0, _per_eye_width, _per_eye_height);
    fbi->pushViewport(extents);
    fbi->pushScissor(extents);
    dwi->fullscreenQuad();
    fbi->popViewport();
    fbi->popScissor();
    mtl.end(framedata);
    fbi->PopRtGroup();

    context->debugPopGroup();
  }
  ///////////////////////////////////////
  std::shared_ptr<StereoCameraMatrices> _stereomatrices;
  std::shared_ptr<CameraMatrices> _tmpcameramatrices;

  DualMonoVrOutputNode* _vrnode = nullptr;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  bool _doinit  = true;
  FreestyleMaterial _blit2screenmtl;
  const FxShaderTechnique* _fxtechnique_downsample[4];
  const FxShaderParam* _fxpMVP;
  const FxShaderParam* _fxpColorMap;
  int _multiplier     = 1;
  int _ssaa_width     = 0;
  int _ssaa_height    = 0;
  int _per_eye_width  = 0;
  int _per_eye_height = 0;
  rtgroup_ptr_t _ssaadownsamplebufferL;
  rtgroup_ptr_t _ssaadownsamplebufferR;
};
using DMVRIMPL_ptr_t = std::shared_ptr<DMVRIMPL>;
///////////////////////////////////////////////////////////////////////////////
DualMonoVrOutputNode::DualMonoVrOutputNode() {
  _impl = std::make_shared<DMVRIMPL>(this);
}
///////////////////////////////////////////////////////////////////////////////
DualMonoVrOutputNode::~DualMonoVrOutputNode() {
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::gpuInit(lev2::Context* pTARG, int iW, int iH) {
  _impl.get<DMVRIMPL_ptr_t>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
compdrawdata_fn_t DualMonoVrOutputNode::createAssembler(nodecompositortechnique_ptr_t tek) {
  return [this, tek](CompositorDrawData& drawdata) {
    auto rnode = tek->_renderNode;
    auto context = drawdata.context();
    // printf("rendering dual-mono-vr!\n");
    ////////////////////////////////////////////////////////////////////////////
    // this assembler will run the render and postfx nodes twice,
    //  once for each eye
    ////////////////////////////////////////////////////////////////////////////
    // for now, so we dont have to change the render and postfx nodes
    //  we will stash each eye in a holding buffer for final compositing
    ////////////////////////////////////////////////////////////////////////////
    auto do_for_eye = [&](uint64_t eye) {
      auto impl    = _impl.get<DMVRIMPL_ptr_t>();
      auto context = drawdata.context();
      auto framedata = drawdata.RCFD();
      ////////////////////////////////////////////////////////////////////////////
      bool is_left_eye  = (eye == "left"_crcu);
      bool is_right_eye = (not is_left_eye);
      framedata->setUserProperty("eye_index"_crcu, int(is_left_eye ? 0 : 1));
      ////////////////////////////////////////////////////////////////////////////
      rtgroup_ptr_t render_outg = rnode ? rnode->GetOutputGroup() : nullptr;
      RtBuffer* render_out      = rnode ? rnode->GetOutput().get() : nullptr;
      drawdata._properties["render_out"_crcu].set<RtBuffer*>(render_out);
      drawdata._properties["render_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
      ////////////////////////////////////////////////////////////////////////////
      // assemble (render) the eye
      ////////////////////////////////////////////////////////////////////////////
      context->debugPushGroup("DualMonoVrOutputNode::beginAssembleEye");
      impl->_beginAssembleEye(drawdata, is_left_eye);
      context->debugPopGroup();
      ////////////////////////////////////////////////////////////////////////////
      rnode->Render(drawdata);
      ////////////////////////////////////////////////////////////////////////////
      context->debugPushGroup("DualMonoVrOutputNode::endAssembleEye");
      impl->_endAssembleEye(drawdata, is_left_eye);
      context->debugPopGroup();
      ////////////////////////////////////////////////////////////////////////////
      // postfx nodes for the eye
      ////////////////////////////////////////////////////////////////////////////
      size_t num_fx_nodes = tek->_postEffectNodes.size();
      for (auto pfxnode : tek->_postEffectNodes) {
        if (pfxnode->_disabled) {
          continue;
        }
        drawdata._properties["postfx_in"_crcu].set<rtgroup_ptr_t>(render_outg);
        pfxnode->Render(drawdata);
        render_outg = pfxnode->GetOutputGroup();
        render_out  = pfxnode->GetOutput().get();
      }
      drawdata._properties["final_out"_crcu].set<RtBuffer*>(render_out);
      drawdata._properties["final_outgroup"_crcu].set<rtgroup_ptr_t>(render_outg);
      ////////////////////////////////////////////////////////////////////////////
      // downsample ?
      ////////////////////////////////////////////////////////////////////////////
      if (render_out) {
        impl->_downsample(drawdata, render_out, is_left_eye);
      }
      ////////////////////////////////////////////////////////////////////////////
    };

    context->debugPushGroup("DualMonoVrOutputNode::assemble");

    if (_onBeginAssemble) {
      _onBeginAssemble(drawdata);
    }

    context->debugPushGroup("DualMonoVrOutputNode::assembleL");
    do_for_eye("left"_crcu);
    context->debugPopGroup();

    context->debugPushGroup("DualMonoVrOutputNode::assembleR");
    do_for_eye("right"_crcu);
    context->debugPopGroup();

    if (_onEndAssemble) {
      _onEndAssemble(drawdata);
    }

    context->debugPopGroup();
  };
}
///////////////////////////////////////////////////////////////////////////////
void DualMonoVrOutputNode::composite(CompositorDrawData& drawdata) {
  drawdata.context()->debugPushGroup("DualMonoVrOutputNode::composite");
  auto impl = _impl.get<DMVRIMPL_ptr_t>();
  /////////////////////////////////////////////////////////////////////////////
  // DMVR compositor
  /////////////////////////////////////////////////////////////////////////////
  Context* context = drawdata.context();
  auto fbi         = context->FBI();
  auto gbi         = context->GBI();
  
  if (auto try_final = drawdata._properties["final_out"_crcu].tryAs<RtBuffer*>()) {
    auto vrdev     = orkidvr::device();
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

        auto& mtl             = impl->_blit2screenmtl;
        auto inp_rtg          = drawdata._properties["final_outgroup"_crcu].get<rtgroup_ptr_t>();
        auto this_buf         = context->FBI()->GetThisBuffer();
        auto tek_nodownsample = impl->_fxtechnique_downsample[0];

        /////////////////////////////////////////////////////
        if (_distortion_lambda) { // Lens distortion ?
        /////////////////////////////////////////////////////
          drawdata.context()->debugPushGroup("DualMonoVrOutputNode::distortion_lambda");
          int out_surface_width  = context->mainSurfaceWidth();
          int out_surface_height = context->mainSurfaceHeight();
          int wd2 = out_surface_width>>1;
          int h = out_surface_height;
          DistortionRect drectL = {
              impl->_ssaadownsamplebufferL->texture(0).get(),
              SRect(wd2, 0, wd2*2, h),
              'L'
          };
          DistortionRect drectR = {
              impl->_ssaadownsamplebufferR->texture(0).get(),
              SRect(0, 0, wd2, h),
              'R'
          };
          //printf("out_surface_width<%d> out_surface_height<%d>\n", out_surface_width, out_surface_height);
          _distortion_lambda(framedata, drectL);
          _distortion_lambda(framedata, drectR);
          drawdata.context()->debugPopGroup();
        /////////////////////////////////////////////////////
        } else { // no lens distortion
        /////////////////////////////////////////////////////
          drawdata.context()->debugPushGroup("DualMonoVrOutputNode::to_hmd");

          mtl.begin(tek_nodownsample, framedata);
          mtl.bindParamMatrix(impl->_fxpMVP, fmtx4::Identity());

          int out_surface_width  = context->mainSurfaceWidth();
          int out_surface_height = context->mainSurfaceHeight();
          // printf("out_surface_width<%d> out_surface_height<%d>\n", out_surface_width, out_surface_height);
          ViewportRect extents(0, 0, out_surface_width, out_surface_height);
          fbi->pushViewport(extents);
          fbi->pushScissor(extents);

          ////////////
          // Downsampled Left Eye -> Output
          ////////////

          auto tex = impl->_ssaadownsamplebufferL->texture(0).get();
          mtl.bindParamTexture(impl->_fxpColorMap, tex);
          if (_flipY) {
            this_buf->Render2dQuadEML(
                fvec4(-1, -1, 1, 2), // xywh
                fvec4(0, 0, 1, 1),   // uvrectA(u,v,w,h)
                fvec4(0, 0, 1, 1));  // uvrectB(u,v,w,h)
          } else {
            this_buf->Render2dQuadEML(
                fvec4(-1, -1, 1, 2), // xywh
                fvec4(0, 1, 1, -1),  // uvrectA(u,v,w,h)
                fvec4(0, 1, 1, -1)); // uvrectB(u,v,w,h)
          }

          ////////////
          // Downsampled Right Eye -> Output
          ////////////

          tex = impl->_ssaadownsamplebufferR->texture(0).get();
          mtl.bindParamTexture(impl->_fxpColorMap, tex);
          if (_flipY) {
            this_buf->Render2dQuadEML(
                fvec4(0, -1, 1, 2), // xywh
                fvec4(0, 0, 1, 1),  // uvrectA(u,v,w,h)
                fvec4(0, 0, 1, 1)); // uvrectB(u,v,w,h)
          } else {
            this_buf->Render2dQuadEML(
                fvec4(0, -1, 1, 2),  // xywh
                fvec4(0, 1, 1, -1),  // uvrectA(u,v,w,h)
                fvec4(0, 1, 1, -1)); // uvrectB(u,v,w,h)
          }

          ////////////
          // done
          ////////////

          fbi->popViewport();
          fbi->popScissor();

          mtl.end(framedata);

          drawdata.context()->debugPopGroup();
        } // no distortion

        drawdata.context()->debugPopGroup();
      }
    }
  }
  drawdata.context()->debugPopGroup();
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
