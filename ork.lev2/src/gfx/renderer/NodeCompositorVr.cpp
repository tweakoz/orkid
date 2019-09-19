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

struct VrFrameTechnique final : public FrameTechniqueBase {
  //////////////////////////////////////////////////////////////////////////////
  VrFrameTechnique(VrCompositingNode* node, int w, int h)
      : FrameTechniqueBase(w, h)
      , _rtg(nullptr)
      , _vrcnode(node){}
  //////////////////////////////////////////////////////////////////////////////
  void DoInit(GfxTarget* pTARG) final {
    if (nullptr == _rtg) {
      _rtg = new RtGroup(pTARG, miW, miH, NUMSAMPLES);

      auto lbuf = new RtBuffer(_rtg, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);

      _rtg->SetMrt(0, lbuf);

      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
  }

  //////////////////////////////////////////////////////////////////////////////
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
  //////////////////////////////////////////////////////////////////////////////
  void renderBothEyes(FrameRenderer& renderer,
                      CompositorDrawData& drawdata,
                      CameraData* lcam,
                      CameraData* rcam,
                      const std::map<int, orkidvr::ControllerState>& controllers) {
    RenderContextFrameData& framedata = renderer.framedata();
    GfxTarget* pTARG                  = framedata.GetTarget();

    SRect tgt_rect(0, 0, miW, miH);

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = this;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = nullptr; // default == "All"
    _CPD._clearColor  = fvec4(0.61, 0.61, 0.75, 1);

    //////////////////////////////////////////////////////
    // is stereo active
    //////////////////////////////////////////////////////

    if (orkidvr::device()._active) {
      framedata.setStereoOnePass(true);
      framedata.setUserProperty("lcam"_crc, lcam);
      framedata.setUserProperty("rcam"_crc, rcam);
      framedata.SetCameraData(lcam);
      _CPD._impl.Set<const CameraData*>(lcam);
    } else {
      lcam->BindGfxTarget(pTARG);
      rcam->BindGfxTarget(pTARG);
      framedata.SetCameraData(lcam);
      _CPD._impl.Set<const CameraData*>(lcam);
    }

    //////////////////////////////////////////////////////
    pTARG->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    //////////////////////////////////////////////////////

    // draw left and right ///////////////////////////////

    RtGroupRenderTarget rtL(_rtg);
    drawdata.mCompositingGroupStack.push(_CPD);
    {
      pTARG->SetRenderContextFrameData(&framedata);
      framedata.SetDstRect(tgt_rect);
      framedata.PushRenderTarget(&rtL);
      pTARG->FBI()->PushRtGroup(_rtg);
      pTARG->BeginFrame();
      framedata.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      renderer.Render();
      // renderPoses(pTARG, lcam, controllers);
      pTARG->EndFrame();
      pTARG->FBI()->PopRtGroup();
      framedata.PopRenderTarget();
      pTARG->SetRenderContextFrameData(nullptr);
      drawdata.mCompositingGroupStack.pop();
    }

    framedata.setStereoOnePass(false);
  }

  RtGroup* _rtg;
  BuiltinFrameEffectMaterial _effect;
  CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
  VrCompositingNode* _vrcnode = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
struct VRIMPL {
  ///////////////////////////////////////
  VRIMPL(VrCompositingNode*node)
      : _vrnode(node)
      , _frametek(nullptr)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {}
  ///////////////////////////////////////
  ~VRIMPL() {
    if (_frametek)
      delete _frametek;
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    _material.Init(pTARG);
    int w     = orkidvr::device()._width;
    int h     = orkidvr::device()._height;
    _frametek = new VrFrameTechnique(_vrnode,w*2, h);
    _frametek->Init(pTARG);
  }
  ///////////////////////////////////////
  void _myrender(FrameRenderer& renderer, CompositorDrawData& drawdata, fmtx4 rootmatrix) {

    RenderContextFrameData& framedata = renderer.framedata();
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

    _frametek->renderBothEyes(renderer, drawdata, &LCAM, &RCAM, CONT);
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
  VrFrameTechnique* _frametek;
  VrCompositingNode* _vrnode = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode() { _impl = std::make_shared<VRIMPL>(this); }
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  auto vrimpl = _impl.Get<std::shared_ptr<VRIMPL>>();

  if (nullptr == vrimpl->_frametek) {
    vrimpl->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingImpl* impl) // virtual
{
  FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
  RenderContextFrameData& framedata = the_renderer.framedata();
  auto targ                               = framedata.GetTarget();

  auto vrimpl                 = _impl.Get<std::shared_ptr<VRIMPL>>();

  //////////////////////////////////////////////
  // find vr camera
  //////////////////////////////////////////////


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

  if (vrimpl->_frametek) {

    vrimpl->_frametek->_viewOffsetMatrix = orkidvr::device()._outputViewOffsetMatrix;

    /////////////////////////////////////////////////////////////////////////////
    // render eyes
    /////////////////////////////////////////////////////////////////////////////

    framedata.setLayerName("All");
    vrimpl->_myrender(the_renderer, drawdata, rootmatrix);

    /////////////////////////////////////////////////////////////////////////////
    // VR compositor
    /////////////////////////////////////////////////////////////////////////////

    auto buffer = vrimpl->_frametek->_rtg->GetMrt(0);
    assert(buffer != nullptr);

    /////////////////////////////////////////////////////////////////////////////

    auto tex = buffer->GetTexture();
    if (tex) {
      orkidvr::composite(targ, tex);
    }

    /////////////////////////////////////////////////////////////////////////////
  }
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
