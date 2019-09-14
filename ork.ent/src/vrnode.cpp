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

constexpr int NUMSAMPLES = 16;

struct VrFrameTechnique final : public FrameTechniqueBase {
  VrFrameTechnique(int w, int h)
      : FrameTechniqueBase(w, h)
      , _rtg_left(nullptr)
      , _rtg_right(nullptr) {}

  void DoInit(GfxTarget* pTARG) final {
    if (nullptr == _rtg_left) {
      _rtg_left  = new RtGroup(pTARG, miW, miH, NUMSAMPLES);
      _rtg_right = new RtGroup(pTARG, miW, miH, NUMSAMPLES);

      auto lbuf = new RtBuffer(_rtg_left, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);
      auto rbuf = new RtBuffer(_rtg_right, lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32, miW, miH);

      _rtg_left->SetMrt(0, lbuf);
      _rtg_right->SetMrt(0, rbuf);

      _effect.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
    }
  }
  void renderBothEyes(FrameRenderer& renderer,
                      CompositorSystemDrawData& drawdata,
                      CameraData* lcam,
                      CameraData* rcam,
                      const std::map<int, orkidvr::ControllerState>& controllers) {
    RenderContextFrameData& FrameData = renderer.GetFrameData();
    GfxTarget* pTARG                  = FrameData.GetTarget();

    SRect tgt_rect(0, 0, miW, miH);

    _CPD.mbDrawSource = true;
    _CPD.mpFrameTek   = this;
    _CPD.mpCameraName = nullptr;
    _CPD.mpLayerName  = nullptr; // default == "All"

    //////////////////////////////////////////////////////
    // render all controller poses
    //////////////////////////////////////////////////////

    auto renderposes = [&](CameraData* camdat) {
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

        pTARG->MTXI()->PushMMatrix(mmtx);
        pTARG->MTXI()->PushVMatrix(camdat->GetVMatrix());
        pTARG->MTXI()->PushPMatrix(camdat->GetPMatrix());
        pTARG->PushModColor(fvec4::White());
        {
          if (c._button2down)
            ork::lev2::GfxPrimitives::GetRef().RenderBox(pTARG);
          else
            ork::lev2::GfxPrimitives::GetRef().RenderAxis(pTARG);
        }
        pTARG->PopModColor();
        pTARG->MTXI()->PopPMatrix();
        pTARG->MTXI()->PopVMatrix();
        pTARG->MTXI()->PopMMatrix();
      }
    };

    //////////////////////////////////////////////////////

    pTARG->FBI()->SetAutoClear(false);
    // clear will occur via _CPD
    // draw left ////////////////////////////////////////

    lcam->BindGfxTarget(pTARG);
    FrameData.SetCameraData(lcam);
    _CPD._impl.Set<const CameraData*>(lcam);
    _CPD._clearColor = fvec4(0.61, 0.61, 0.71, 1);

    RtGroupRenderTarget rtL(_rtg_left);
    drawdata.mCompositingGroupStack.push(_CPD);
    {
      pTARG->SetRenderContextFrameData(&FrameData);
      FrameData.SetDstRect(tgt_rect);
      FrameData.PushRenderTarget(&rtL);
      pTARG->FBI()->PushRtGroup(_rtg_left);
      pTARG->BeginFrame();
      FrameData.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      renderer.Render();
      renderposes(lcam);
      pTARG->EndFrame();
      pTARG->FBI()->PopRtGroup();
      FrameData.PopRenderTarget();
      pTARG->SetRenderContextFrameData(nullptr);
      drawdata.mCompositingGroupStack.pop();
    }

    // draw right ///////////////////////////////////////

    rcam->BindGfxTarget(pTARG);
    FrameData.SetCameraData(rcam);
    _CPD._impl.Set<const CameraData*>(rcam);
    //_CPD._clearColor = fvec4(0, 0, .1, 1);

    drawdata.mCompositingGroupStack.push(_CPD);
    {
      RtGroupRenderTarget rtR(_rtg_right);
      pTARG->SetRenderContextFrameData(&FrameData);
      FrameData.SetDstRect(tgt_rect);
      FrameData.PushRenderTarget(&rtR);
      pTARG->FBI()->PushRtGroup(_rtg_right);
      pTARG->BeginFrame();
      FrameData.SetRenderingMode(RenderContextFrameData::ERENDMODE_STANDARD);
      renderer.Render();
      renderposes(rcam);
      pTARG->EndFrame();
      pTARG->FBI()->PopRtGroup();
      FrameData.PopRenderTarget();
      pTARG->SetRenderContextFrameData(nullptr);
    }
  }

  RtGroup* _rtg_left;
  RtGroup* _rtg_right;
  BuiltinFrameEffectMaterial _effect;
  ent::CompositingPassData _CPD;
  fmtx4 _viewOffsetMatrix;
};

///////////////////////////////////////////////////////////////////////////////
struct VRSYSTEMIMPL {
  ///////////////////////////////////////
  VRSYSTEMIMPL()
      : _frametek(nullptr)
      , _camname(AddPooledString("Camera"))
      , _layers(AddPooledString("All")) {}
  ///////////////////////////////////////
  ~VRSYSTEMIMPL() {

    if (_frametek)
      delete _frametek;
  }
  ///////////////////////////////////////
  void init(lev2::GfxTarget* pTARG) {
    _material.Init(pTARG);
    int w     = orkidvr::device()._width;
    int h     = orkidvr::device()._height;
    _frametek = new VrFrameTechnique(w, h);
    _frametek->Init(pTARG);
  }
  ///////////////////////////////////////
  void _myrender(Simulation* psi, FrameRenderer& renderer, CompositorSystemDrawData& drawdata, fmtx4 rootmatrix) {

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
VrCompositingNode::VrCompositingNode() { _impl = std::make_shared<VRSYSTEMIMPL>(); }
///////////////////////////////////////////////////////////////////////////////
VrCompositingNode::~VrCompositingNode() {}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoInit(lev2::GfxTarget* pTARG, int iW, int iH) // virtual
{
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();

  if (nullptr == vrimpl->_frametek) {
    vrimpl->init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void VrCompositingNode::DoRender(CompositorSystemDrawData& drawdata, CompositingSystem* compsys) // virtual
{
  lev2::FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
  lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
  auto targ                               = framedata.GetTarget();

  auto vrimpl                 = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();
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

    anyp PassData;
    PassData.Set<const char*>("All");
    the_renderer.GetFrameData().SetUserProperty("pass", PassData);
    vrimpl->_myrender(psi, the_renderer, drawdata, rootmatrix);

    /////////////////////////////////////////////////////////////////////////////
    // VR compositor
    /////////////////////////////////////////////////////////////////////////////

    auto bufferL = vrimpl->_frametek->_rtg_left->GetMrt(0);
    assert(bufferL != nullptr);
    auto bufferR = vrimpl->_frametek->_rtg_right->GetMrt(0);
    assert(bufferR != nullptr);

    auto texL = bufferL->GetTexture();
    auto texR = bufferR->GetTexture();
    if (texL && texR) {
      orkidvr::composite(targ, texL, texR);
    }

    /////////////////////////////////////////////////////////////////////////////
  }
}
///////////////////////////////////////////////////////////////////////////////
lev2::RtGroup* VrCompositingNode::GetOutput() const {
  auto vrimpl = _impl.Get<std::shared_ptr<VRSYSTEMIMPL>>();
  if (vrimpl->_frametek)
    return vrimpl->_frametek->_rtg_left;
  else
    return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::ent
