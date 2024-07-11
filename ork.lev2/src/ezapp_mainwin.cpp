#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/vr/vr.h>
#include <ork/lev2/imgui/imgui.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
#include <ork/lev2/imgui/imgui_impl_opengl3.h>
#include <boost/program_options.hpp>
#include <ork/kernel/environment.h>
#include <ork/util/logger.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
EzMainWin::EzMainWin(OrkEzApp& app)
    : _app(app) {
  _execsceneparams   = std::make_shared<varmap::VarMap>();
  _update_rendersync = app._initdata->_update_rendersync;
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::enqueueWindowResize(int w, int h){
  _ctqt->enqueueWindowResize(w, h);
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_updateEnqueueLockedAndReleaseFrame(DrawQueue* dbuf) {
  // if(_app._initdata->_update_rendersync){
  // DrawQueue::releaseFromWriteLocked(dbuf);
  //}
  // else{
  // DrawQueue::releaseFromWrite(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_updateEnqueueUnlockedAndReleaseFrame(DrawQueue* dbuf) {
  // if(_app._initdata->_update_rendersync){
  // DrawQueue::releaseFromWriteLocked(dbuf);
  //}
  // else{
  // DrawQueue::releaseFromWrite(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
const DrawQueue* EzMainWin::_tryAcquireDrawBuffer(ui::drawevent_constptr_t drawEvent) {
  //_curframecontext = drawEvent->GetTarget();

  // if(_app._initdata->_update_rendersync){
  // return DrawQueue::acquireForReadLocked();
  //}
  // else {
  // return DrawQueue::acquireForRead(7);
  //
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
DrawQueue* EzMainWin::_tryAcquireUpdateBuffer() {
  DrawQueue* rval = nullptr;
  // if(_app._initdata->_update_rendersync){
  // rval = DrawQueue::acquireForWriteLocked();
  //}
  // else {
  // rval = DrawQueue::acquireForWrite(7);
  //}
  // rval->Reset();
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_releaseAcquireUpdateBuffer(DrawQueue*db){
  // if(_app._initdata->_update_rendersync){
  //    DrawQueue::releaseFromWriteLocked(db);
  //}
  // else {
  // DrawQueue::releaseFromWrite(db);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_beginFrame(const DrawQueue* dbuf) {
  auto try_ctx = dbuf->getUserProperty("CONTEXT"_crcu);
  _curframecontext->beginFrame();
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_endFrame(const DrawQueue* dbuf) {
  if (_update_rendersync) {
    // auto do_rlock = dbuf->getUserProperty("RENDERLOCK"_crcu);
    // if (auto as_lock = do_rlock.tryAs<atom_rlock_ptr_t>()) {
    // as_lock.value()->store(1);
    //}
  }
  _curframecontext->endFrame();
  // if(_app._initdata->_update_rendersync){
  // DrawQueue::releaseFromReadLocked(dbuf);
  //}
  // else{
  // DrawQueue::releaseFromRead(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::withAcquiredDrawQueueForUpdate(int debugcode, std::function<void(const AcquiredDrawQueueForUpdate&)> l) {
  DrawQueue* DB = nullptr;

  if (_update_rendersync) {
    // DB = DrawQueue::acquireForWriteLocked();
  } else {
    // DB = DrawQueue::acquireForWrite(debugcode);
  }

  if (DB) {
    DB->Reset();
    AcquiredDrawQueueForUpdate udb;
    udb._DB = DB;
    l(udb);
    if (_update_rendersync)
      _updateEnqueueLockedAndReleaseFrame(DB);
    else
      _updateEnqueueUnlockedAndReleaseFrame(DB);
  }
}
///////////////////////////////////////////////////////////////////////////////
EzMainWin::~EzMainWin() {
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2 {
