////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/vr/vr.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/Compositor.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/input.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::VrCompositingNode, "VrCompositingNode");

using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::Describe() {}

///////////////////////////////////////////////////////////////////////////

constexpr int NUMSAMPLES = 1;

struct VrFrameTechnique final : public FrameTechniqueBase {
  //////////////////////////////////////////////////////////////////////////////
  VrFrameTechnique(int w, int h)
      : FrameTechniqueBase(w, h)
      , _rtg(nullptr) {}
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
    RenderContextFrameData& framedata = renderer.GetFrameData();
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
  ent::CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
};

///////////////////////////////////////////////////////////////////////////////
struct VRIMPL {
  ///////////////////////////////////////
  VRIMPL()
      : _frametek(nullptr)
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
    _frametek = new VrFrameTechnique(w*2, h);
    _frametek->Init(pTARG);
  }
  ///////////////////////////////////////
  void _myrender(Simulation* psi, FrameRenderer& renderer, CompositorDrawData& drawdata, fmtx4 rootmatrix) {

    auto playerspawn = psi->FindEntity(AddPooledString("playerspawn"));
    auto playermtx   = playerspawn->GetEffectiveMatrix();

    orkidvr::gpuUpdate(playermtx);

    auto& LCAM = orkidvr::device()._leftcamera;
    auto& RCAM = orkidvr::device()._rightcamera;
    auto& CONT = orkidvr::device()._controllers;

    _frametek->renderBothEyes(renderer, drawdata, &LCAM, &RCAM, CONT);
  }
  ///////////////////////////////////////
  PoolString _camname, _layers;
  CompositingMaterial _material;
  VrFrameTechnique* _frametek;
  InputSystem* _inputsys = nullptr;
};
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::VrCompositingNode() { _impl = std::make_shared<VRIMPL>(); }
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
void VrCompositingNode::DoRender(CompositorDrawData& drawdata, CompositingSystem* compsys) // virtual
{
  lev2::FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
  auto targ                               = framedata.GetTarget();

  auto vrimpl                 = _impl.Get<std::shared_ptr<VRIMPL>>();
  static PoolString vrcamname = AddPooledString("vrcam");

  //////////////////////////////////////////////
  // find vr camera
  //////////////////////////////////////////////

  auto psi   = compsys->simulation();
  auto vrcam = psi->GetCameraData(vrcamname);

  fmtx4 rootmatrix;

  if (vrcam != nullptr) {
    auto eye = vrcam->GetEye();
    auto tgt = vrcam->GetTarget();
    auto up  = vrcam->GetUp();
    rootmatrix.LookAt(eye, tgt, up);
  }

  if (vrimpl->_frametek) {

    vrimpl->_frametek->_viewOffsetMatrix = orkidvr::device()._outputViewOffsetMatrix;

    /////////////////////////////////////////////////////////////////////////////
    // render eyes
    /////////////////////////////////////////////////////////////////////////////

    rendervar_t passdata;
    passdata.Set<const char*>("All");
    the_renderer.GetFrameData().setUserProperty("pass"_crc, passdata);
    vrimpl->_myrender(psi, the_renderer, drawdata, rootmatrix);

    /////////////////////////////////////////////////////////////////////////////
    // VR compositor
    /////////////////////////////////////////////////////////////////////////////

    auto buffer = vrimpl->_frametek->_rtg->GetMrt(0);
    assert(buffer != nullptr);
    auto tex = buffer->GetTexture();
    if (tex) {
      orkidvr::composite(targ, tex);
    }

    /////////////////////////////////////////////////////////////////////////////
  }
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtGroup* VrCompositingNode::GetOutput() const {
  auto vrimpl = _impl.Get<std::shared_ptr<VRIMPL>>();
  if (vrimpl->_frametek)
    return vrimpl->_frametek->_rtg;
  else
    return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
