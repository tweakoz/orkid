///////////////////////////////////////////////////////////////////////////////
// Visual loading progress screen
//   TODO: themes..
//   TODO: stereo version..
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/object/AutoConnector.h>
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CTXBASE, "Lev2CTXBASE");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
struct CtxBaseProgressPimpl { //
  CtxBaseProgressPimpl(Context* context) {
    _material      = std::make_shared<GfxMaterialUITextured>(context);
    auto& rasstate = _material->_rasterstate;
    rasstate.SetDepthTest(EDEPTHTEST_OFF);
    rasstate.SetBlending(Blending::OFF);
    rasstate.SetAlphaTest(EALPHATEST_OFF, 0.0f);
    rasstate.SetDepthTest(EDEPTHTEST_ALWAYS);
    auto txi                                                    = context->TXI();
    _loadingtex                                                 = std::make_shared<Texture>();
    _loadingtex->_varmap.makeValueForKey<bool>("loadimmediate") = true;
    txi->LoadTexture("data://misc/orkidprogressbg", _loadingtex.get());
    _material->SetTexture(ETEXDEST_DIFFUSE, _loadingtex.get());
  }
  std::shared_ptr<Texture> _loadingtex;
  std::shared_ptr<GfxMaterialUITextured> _material;
};
using progresspimpl_ptr_t = std::shared_ptr<CtxBaseProgressPimpl>;
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::Describe() {
  RegisterAutoSlot(ork::lev2::CTXBASE, Repaint);
}
///////////////////////////////////////////////////////////////////////////////
CTXBASE::CTXBASE(Window* pwin)
    : mbInitialize(true)
    , mpWindow(pwin)
    , _target(0)
    , ConstructAutoSlot(Repaint) {

  AutoConnector::setupSignalsAndSlots(this);
  mpWindow->mpCTXBASE = this;

  _uievent = std::make_shared<ui::Event>();
}
///////////////////////////////////////////////////////////////////////////////
CTXBASE::~CTXBASE() {
  if (mpWindow)
    delete mpWindow;
}
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::pushRefreshPolicy(RefreshPolicyItem policy) {
  _policyStack.push(_curpolicy);
  _setRefreshPolicy(policy);
}
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::popRefreshPolicy() {
  auto prev = _policyStack.top();
  _setRefreshPolicy(prev);
}
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::progressHandler(opq::progressdata_ptr_t data) {

  if (nullptr != _target) {
    if (auto as_pimpl = _pimpl_progress.TryAs<progresspimpl_ptr_t>()) {
      auto pimpl                            = as_pimpl.value();
      DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();
      int TARGW                             = _target->mainSurfaceWidth();
      int TARGH                             = _target->mainSurfaceHeight();

      float target_aspect = float(TARGW) / float(TARGH);
      float image_aspect  = float(pimpl->_loadingtex->_width) //
                           / float(pimpl->_loadingtex->_height);

      const auto tgtrect = ViewportRect(0, 0, TARGW, TARGH);
      ////////////////////////////////////////////////
      lev2::RenderContextFrameData RCFD(_target);
      _target->pushRenderContextFrameData(&RCFD);
      /////////////////////////////////
      lev2::CompositingPassData TOPCPD;
      TOPCPD.SetDstRect(tgtrect);
      TOPCPD.SetDstRect(tgtrect);
      static CompositingData _gdata;
      static auto _gimpl = _gdata.createImpl();
      RCFD._cimpl        = _gimpl;
      _gimpl->pushCPD(TOPCPD);
      /////////////////////////////////
      auto FBI  = _target->FBI();
      auto MTXI = _target->MTXI();
      FBI->SetAutoClear(true);
      static float phi = 0.0f;
      phi += 0.5f;
      float r         = 0.5 * sinf(phi * 0.1) + 0.5;
      float g         = 0.5 * sinf(phi * 0.17) + 0.5;
      float b         = 0.5 * sinf(phi * 0.23) + 0.5;
      auto clearcolor = fvec4(r, g, b, 1.0f);
      FBI->SetClearColor(clearcolor);
      /////////////////////////////////
      // throw away any drawable buffers
      /////////////////////////////////
      if (auto DB = DrawableBuffer::acquireForRead(7))
        DrawableBuffer::releaseFromRead(DB);
      /////////////////////////////////
      _target->beginFrame();
      FBI->setViewport(tgtrect);
      FBI->setScissor(tgtrect);

      /////////////////////////////////////////
      // background
      /////////////////////////////////////////
      MTXI->PushMMatrix(fmtx4());
      MTXI->PushUIMatrix();
      _target->PushModColor(clearcolor);
      mpWindow->RenderMatOrthoQuad(
          tgtrect.asSRect(), //
          tgtrect.asSRect(),
          pimpl->_material.get());
      _target->PopModColor();
      /////////////////////////////////////////
      // messages
      /////////////////////////////////////////
      auto formatteda = FormatString("opq: %s", data->_queue_name.c_str());
      auto formattedb = FormatString("completion_group: %s", data->_task_name.c_str());
      auto formattedc = FormatString("ops pending: %d", data->_num_pending);
      int y           = (TARGH / 2);
      FontMan::PushFont("i48");

      _target->PushModColor(fcolor4(1.0, 1.0, 1.0));
      FontMan::GetRef().beginTextBlock(_target);
      FontMan::DrawCenteredText(_target, y - 48, formatteda.c_str());
      FontMan::GetRef().endTextBlock(_target);
      _target->PopModColor();

      _target->PushModColor(fcolor4(1.0, 1.0, 0.5));
      FontMan::GetRef().beginTextBlock(_target);
      FontMan::DrawCenteredText(_target, y + 0, formattedb.c_str());
      FontMan::GetRef().endTextBlock(_target);
      _target->PopModColor();

      _target->PushModColor(fcolor4(1.0, 0.7, 0.3));
      FontMan::GetRef().beginTextBlock(_target);
      FontMan::DrawCenteredText(_target, y + 48, formattedc.c_str());
      FontMan::GetRef().endTextBlock(_target);
      _target->PopModColor();

      FontMan::PopFont();
      /////////////////////////////////////////

      MTXI->PopUIMatrix();
      MTXI->PopMMatrix();

      _target->endFrame();
      _target->popRenderContextFrameData();
      _gimpl->popCPD();

    } else {
      auto pimpl = _pimpl_progress.makeShared<CtxBaseProgressPimpl>(_target);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
CTFLXID CTXBASE::GetThisXID(void) const {
  return mxidThis;
}
CTFLXID CTXBASE::GetTopXID(void) const {
  return mxidTopLevel;
}
void CTXBASE::SetThisXID(CTFLXID xid) {
  mxidThis = xid;
}
void CTXBASE::SetTopXID(CTFLXID xid) {
  mxidTopLevel = xid;
}
Context* CTXBASE::GetTarget() const {
  return _target;
}
Window* CTXBASE::GetWindow() const {
  return mpWindow;
}
void CTXBASE::setContext(Context* ctx) {
  _target            = ctx;
  _uievent->_context = ctx;
}
void CTXBASE::SetWindow(Window* pw) {
  mpWindow = pw;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
