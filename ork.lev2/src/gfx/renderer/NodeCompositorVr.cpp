////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "NodeCompositorVr.h"
#include <ork/application/application.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/gfx/gfxprimitives.h>

ImplementReflectionX(ork::lev2::VrCompositingNode, "VrCompositingNode");

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::describeX(class_t*c) {
}
///////////////////////////////////////////////////////////////////////////
constexpr int NUMSAMPLES = 1;
///////////////////////////////////////////////////////////////////////////////
struct VRIMPL {
  ///////////////////////////////////////
  VRIMPL(VrCompositingNode*node)
      : _vrnode(node)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")){}
  ///////////////////////////////////////
  ~VRIMPL() {}
  ///////////////////////////////////////
  void gpuInit(lev2::GfxTarget* pTARG) {
    _material.Init(pTARG);
    _width     = orkidvr::device()._width*2;
    _height     = orkidvr::device()._height;
    if (nullptr == _rtg) {
      _rtg = new RtGroup(pTARG, _width, _height, NUMSAMPLES);
      _vrrendertarget = new RtGroupRenderTarget(_rtg);
      auto lbuf = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, _width, _height);

      _rtg->SetMrt(0, lbuf);

      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
  }
  ///////////////////////////////////////
  typedef const std::map<int, orkidvr::ControllerState>& controllermap_t;
  void renderPoses(GfxTarget* targ, CameraData* camdat, controllermap_t controllers) {
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
  void beginFrame(CompositorDrawData& drawdata){

    FrameRenderer& framerenderer       = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = framerenderer.framedata();
    GfxTarget* pTARG                  = framedata.GetTarget();

    /////////////////////////////////////////////////////////////////////////////
    // get VR camera
    /////////////////////////////////////////////////////////////////////////////

    auto vrcamprop = framedata.getUserProperty("vrcam"_crc);
    fmtx4 rootmatrix;
    if( auto as_cam = vrcamprop.TryAs<const CameraData*>() ){
      auto vrcam = as_cam.value();
      auto eye = vrcam->GetEye();
      auto tgt = vrcam->GetTarget();
      auto up  = vrcam->GetUp();
      rootmatrix.LookAt(eye, tgt, up);
    }
    else{
      printf("vrcamtype<%s>\n", vrcamprop.GetTypeName() );
    }

    _viewOffsetMatrix = orkidvr::device()._outputViewOffsetMatrix;

    /////////////////////////////////////////////////////////////////////////////
    // render eyes
    /////////////////////////////////////////////////////////////////////////////

    framedata.setLayerName("All");

    auto vrroot = framedata.getUserProperty("vrroot"_crc);
    if( auto as_mtx = vrroot.TryAs<fmtx4>() ){
      orkidvr::gpuUpdate(as_mtx.value());
    }
    else{
      printf("vrroottype<%s>\n", vrroot.GetTypeName() );
    }

    auto& LCAM = orkidvr::device()._leftcamera;
    auto& RCAM = orkidvr::device()._rightcamera;
    auto& CONT = orkidvr::device()._controllers;

    ///////////////////////////////////

    SRect tgt_rect(0, 0, _width, _height);

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = nullptr;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = nullptr; // default == "All"
    _CPD._clearColor  = fvec4(0.61,0,0, 1);

    //////////////////////////////////////////////////////
    // is stereo active
    //////////////////////////////////////////////////////

    if (orkidvr::device()._active) {
      framedata.setStereoOnePass(true);
      framedata.setUserProperty("lcam"_crc, (const CameraData*) &LCAM);
      framedata.setUserProperty("rcam"_crc, (const CameraData*) &RCAM);
      framedata.SetCameraData(&LCAM);
      _CPD._impl.Set<const CameraData*>(&LCAM);
    } else {
      LCAM.BindGfxTarget(pTARG);
      RCAM.BindGfxTarget(pTARG);
      framedata.SetCameraData(&LCAM);
      _CPD._impl.Set<const CameraData*>(&LCAM);
    }

    //////////////////////////////////////////////////////
    pTARG->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    //////////////////////////////////////////////////////

    // draw left and right ///////////////////////////////

    drawdata.mCompositingGroupStack.push(_CPD);
      pTARG->SetRenderContextFrameData(&framedata);
      framedata.SetDstRect(tgt_rect);
      framedata.PushRenderTarget(_vrrendertarget);
      pTARG->FBI()->PushRtGroup(_rtg);
      pTARG->BeginFrame();
      framedata.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
  }
  ///////////////////////////////////////
  void endFrame( CompositorDrawData& drawdata, RtGroup* final){
    FrameRenderer& framerenderer       = drawdata.mFrameRenderer;
    RenderContextFrameData& framedata = framerenderer.framedata();
    GfxTarget* pTARG                  = framedata.GetTarget();
    pTARG->EndFrame();
    pTARG->FBI()->PopRtGroup();
    framedata.PopRenderTarget();
    pTARG->SetRenderContextFrameData(nullptr);
    drawdata.mCompositingGroupStack.pop();
    framedata.setStereoOnePass(false);
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
  VrCompositingNode* _vrnode = nullptr;
  RtGroup* _rtg = nullptr;
  RtGroupRenderTarget* _vrrendertarget;
  BuiltinFrameEffectMaterial _effect;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  int _width = 0;
  int _height = 0;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode() { _impl = std::make_shared<VRIMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::gpuInit(lev2::GfxTarget* pTARG, int iW, int iH)
{ _impl.Get<std::shared_ptr<VRIMPL>>()->gpuInit(pTARG);
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::beginFrame(CompositorDrawData& drawdata, CompositingImpl* cimpl)
{ auto vrimpl = _impl.Get<std::shared_ptr<VRIMPL>>();
  vrimpl->beginFrame(drawdata);
}
void VrCompositingNode::endFrame(CompositorDrawData& drawdata, CompositingImpl* impl,RtGroup* final){
  _impl.Get<std::shared_ptr<VRIMPL>>()->endFrame(drawdata,final);
  /////////////////////////////////////////////////////////////////////////////
  // VR compositor
  /////////////////////////////////////////////////////////////////////////////
  FrameRenderer& framerenderer       = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata  = framerenderer.framedata();
  GfxTarget* targ                    = framedata.GetTarget();
  if( final ){
    auto buffer = final->GetMrt(0);
    if( buffer ){
      assert(buffer != nullptr);
      auto tex = buffer->GetTexture();
      if (tex) {
        orkidvr::composite(targ, tex);
      }
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
