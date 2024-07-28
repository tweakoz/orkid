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
//#include <ork/reflect/Functor.inl>
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CTXBASE, "Lev2CTXBASE");
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
struct CtxBaseProgressPimpl { //
  CtxBaseProgressPimpl(Context* context) {
    _material      = std::make_shared<GfxMaterialUITextured>(context);
    auto& rasstate = _material->_rasterstate;
    rasstate.SetDepthTest(EDepthTest::OFF);
    rasstate.SetBlending(Blending::OFF);
    rasstate.SetAlphaTest(EALPHATEST_OFF, 0.0f);
    rasstate.SetDepthTest(EDepthTest::ALWAYS);
    auto txi                                                    = context->TXI();
    _loadingtex                                                 = std::make_shared<Texture>();
    _loadingtex->_vars->makeValueForKey<bool>("loadimmediate") = true;
    txi->LoadTexture("data://misc/orkidprogressbg", _loadingtex);
    _material->SetTexture(ETEXDEST_DIFFUSE, _loadingtex.get());
  }
  std::shared_ptr<Texture> _loadingtex;
  std::shared_ptr<GfxMaterialUITextured> _material;
};
using progresspimpl_ptr_t = std::shared_ptr<CtxBaseProgressPimpl>;
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::Describe() {
  //RegisterAutoSlot(ork::lev2::CTXBASE, Repaint);
  
}
void CTXBASE::onSharedCreate(std::shared_ptr<CTXBASE> shared_this){
  //AutoConnector::setupSignalsAndSlots(shared_this);
  //attachAutoSlot(Repaint);
  //shared_this->_slotRepaint->attach(shared_this);
}
///////////////////////////////////////////////////////////////////////////////
CTXBASE::CTXBASE(Window* pwin) 
    : _orkwindow(pwin) {
  _slotRepaint = std::make_shared<object::AutoSlot>("repaint");
  if(_orkwindow)
    _orkwindow->mpCTXBASE = this;
  _uievent = std::make_shared<ui::Event>();
}
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::enqueueWindowResize( int w, int h ){
  _doEnqueueWindowResize(w,h);
}
///////////////////////////////////////////////////////////////////////////////
bool CTXBASE::isGlobal() const {
  return (_orkwindow==nullptr);
}
///////////////////////////////////////////////////////////////////////////////
CTXBASE::~CTXBASE() {
  if (_orkwindow)
    delete _orkwindow;
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
RefreshPolicyItem CTXBASE::currentRefreshPolicy() const{
  RefreshPolicyItem rval = _policyStack.top();
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void CTXBASE::progressHandler(opq::progressdata_ptr_t data) {

  if (nullptr != _target) {

    if (auto as_pimpl = _pimpl_progress.tryAs<progresspimpl_ptr_t>()) {
      auto pimpl                            = as_pimpl.value();
      int TARGW                             = _target->mainSurfaceWidth();
      int TARGH                             = _target->mainSurfaceHeight();

      float target_aspect = float(TARGW) / float(TARGH);
      float image_aspect  = float(pimpl->_loadingtex->_width) //
                           / float(pimpl->_loadingtex->_height);

      const auto tgtrect = ViewportRect(0, 0, TARGW, TARGH);
      ////////////////////////////////////////////////
      auto RCFD = std::make_shared<lev2::RenderContextFrameData>(_target);
      _target->pushRenderContextFrameData(RCFD);
      /////////////////////////////////
      lev2::CompositingPassData TOPCPD;
      TOPCPD.SetDstRect(tgtrect);
      TOPCPD.SetDstRect(tgtrect);
      static CompositingData _gdata;
      static auto _gimpl = _gdata.createImpl();
      RCFD->pushCompositor(_gimpl);
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
      //if (auto DB = DrawQueue::acquireForRead(7))
        //DrawQueue::releaseFromRead(DB);
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
      if(_orkwindow){
        makeCurrent();
        _orkwindow->RenderMatOrthoQuad(
          tgtrect.asSRect(), //
          tgtrect.asSRect(),
          pimpl->_material.get());
      }
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
      RCFD->popCompositor();

    } else {
      auto pimpl = _pimpl_progress.makeShared<CtxBaseProgressPimpl>(_target);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
Context* CTXBASE::GetTarget() const {
  return _target;
}
Window* CTXBASE::GetWindow() const {
  return _orkwindow;
}
void CTXBASE::setContext(Context* ctx) {
  _target            = ctx;
  _uievent->_context = ctx;
}
void CTXBASE::SetWindow(Window* pw) {
  _orkwindow = pw;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
