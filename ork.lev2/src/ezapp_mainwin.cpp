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
void EzMainWin::_updateEnqueueLockedAndReleaseFrame(DrawableBuffer* dbuf) {
  // if(_app._initdata->_update_rendersync){
  // DrawableBuffer::releaseFromWriteLocked(dbuf);
  //}
  // else{
  // DrawableBuffer::releaseFromWrite(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_updateEnqueueUnlockedAndReleaseFrame(DrawableBuffer* dbuf) {
  // if(_app._initdata->_update_rendersync){
  // DrawableBuffer::releaseFromWriteLocked(dbuf);
  //}
  // else{
  // DrawableBuffer::releaseFromWrite(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
const DrawableBuffer* EzMainWin::_tryAcquireDrawBuffer(ui::drawevent_constptr_t drawEvent) {
  //_curframecontext = drawEvent->GetTarget();

  // if(_app._initdata->_update_rendersync){
  // return DrawableBuffer::acquireForReadLocked();
  //}
  // else {
  // return DrawableBuffer::acquireForRead(7);
  //
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
DrawableBuffer* EzMainWin::_tryAcquireUpdateBuffer() {
  DrawableBuffer* rval = nullptr;
  // if(_app._initdata->_update_rendersync){
  // rval = DrawableBuffer::acquireForWriteLocked();
  //}
  // else {
  // rval = DrawableBuffer::acquireForWrite(7);
  //}
  // rval->Reset();
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_releaseAcquireUpdateBuffer(DrawableBuffer*db){
  // if(_app._initdata->_update_rendersync){
  //    DrawableBuffer::releaseFromWriteLocked(db);
  //}
  // else {
  // DrawableBuffer::releaseFromWrite(db);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_beginFrame(const DrawableBuffer* dbuf) {
  auto try_ctx = dbuf->getUserProperty("CONTEXT"_crcu);
  _curframecontext->beginFrame();
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::_endFrame(const DrawableBuffer* dbuf) {
  if (_update_rendersync) {
    // auto do_rlock = dbuf->getUserProperty("RENDERLOCK"_crcu);
    // if (auto as_lock = do_rlock.tryAs<atom_rlock_ptr_t>()) {
    // as_lock.value()->store(1);
    //}
  }
  _curframecontext->endFrame();
  // if(_app._initdata->_update_rendersync){
  // DrawableBuffer::releaseFromReadLocked(dbuf);
  //}
  // else{
  // DrawableBuffer::releaseFromRead(dbuf);
  //}
}
///////////////////////////////////////////////////////////////////////////////
void EzMainWin::withAcquiredUpdateDrawBuffer(int debugcode, std::function<void(const AcquiredUpdateDrawBuffer&)> l) {
  DrawableBuffer* DB = nullptr;

  if (_update_rendersync) {
    // DB = DrawableBuffer::acquireForWriteLocked();
  } else {
    // DB = DrawableBuffer::acquireForWrite(debugcode);
  }

  if (DB) {
    DB->Reset();
    AcquiredUpdateDrawBuffer udb;
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
