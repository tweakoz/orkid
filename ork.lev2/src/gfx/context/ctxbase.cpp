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
    : _target(nullptr)
    , _orkwindow(pwin)
    , _needsInitialize(true) {

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
