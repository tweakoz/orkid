////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/ctxbase.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
////////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/imgui/imgui_impl_glfw.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/msgrouter.inl>
#include <ork/math/basicfilters.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
extern bool _macosUseHIDPI;
#endif
///////////////////////////////////////////////////////////////////////////////
static fvec2 gpos;
///////////////////////////////////////////////////////////////////////////////
ui::event_ptr_t CtxGLFW::uievent() {
  return _uievent;
}
///////////////////////////////////////////////////////////////////////////////
ui::event_constptr_t CtxGLFW::uievent() const {
  return _uievent;
}
///////////////////////////////////////////////////////////////////////////////
static void _fire_ui_event(CtxGLFW* ctx){
  auto uiev = ctx->uievent();
  auto gfxwin      = uiev->mpGfxWin;
  auto root        = gfxwin ? gfxwin->GetRootWidget() : nullptr;
  uiev->_uicontext = root ? root->_uicontext : nullptr;
  if (root) {
    uiev->setvpDim(root);
    ui::Event::sendToContext(uiev);
    //_pushTimer.Start();
  }
  //ctx->SlotRepaint(); // refresh UI after button event
}
///////////////////////////////////////////////////////////////////////////////
inline int to_qtmillis(RefreshPolicyItem policy) {
  int user_millis = 0;

  if (policy._fps >= 0)
    user_millis = (policy._fps <= 0) ? 2000 : int(1000.0f / float(policy._fps));

  int qt_millis = 0;

  switch (policy._policy) {
    case EREFRESH_FASTEST:
      qt_millis = 0;
      break;
    case EREFRESH_WHENDIRTY:
      qt_millis = -1;
      break;
    case EREFRESH_FIXEDFPS:
      qt_millis = user_millis + 1;
      break;
    default:
      break;
  }
  return qt_millis;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_setRefreshPolicy(RefreshPolicyItem newpolicy) { // final

  auto prev         = _curpolicy;
  int prev_qtmillis = to_qtmillis(prev);
  int next_qtmillis = to_qtmillis(newpolicy);

  if (next_qtmillis != prev_qtmillis) {
    if (next_qtmillis == -1) {
      // Timer().stop();
    } else {
      // Timer().start();
      // Timer().setInterval(next_qtmillis);
    }
  }

  _curpolicy = newpolicy;
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_refresh(GLFWwindow* window) {
  auto ctx              = (CtxGLFW*)glfwGetWindowUserPointer(window);
  auto orkwin           = ctx->GetWindow();
  static int gistackctr = 0;
  static int gictr      = 0;

  gistackctr++;
  if ((1 == gistackctr) && (gictr > 0)) {
    ctx->uievent()->mpBlindEventData = (void*)nullptr;
    //ctx->SlotRepaint();
  }
  gistackctr--;
  gictr++;
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_winresized(GLFWwindow* window, int w, int h) {
#if defined(__APPLE__)
  if (_macosUseHIDPI) {
    w *= 2;
    h *= 2;
  }
#endif

  //printf("win resized<%p %d %d>\n", window, w, h);
  auto ctx = (CtxGLFW*)glfwGetWindowUserPointer(window);
  ctx->onResize(w, h);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_fbresized(GLFWwindow* window, int w, int h) {
#if defined(__APPLE__)
  if (_macosUseHIDPI) {
    w *= 2;
    h *= 2;
  }
#endif
  //printf("fb resized<%p %d %d>\n", window, w, h);
  auto ctx = (CtxGLFW*)glfwGetWindowUserPointer(window);
  ctx->onResize(w, h);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_contentScaleChanged(GLFWwindow* window, float sw, float sh) {
  //printf("fb contentscale<%p %f %f>\n", window, sw, sh);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_focusChanged(GLFWwindow* window, int focus) {
  bool has_focus = (focus == GLFW_TRUE);

  ////////////////////////
  // TODO - resolve where to send input, IMGUI - or ork::lev2::ui ?
  ////////////////////////

  //ImGui_ImplGlfw_WindowFocusCallback(window, focus);

  ////////////////////////
  //printf("fb focus<%p %d>\n", window, focus);
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_keyboard(GLFWwindow* window, int key, int scancode, int action, int modifiers) {
  //printf("_glfw_callback_keyboard<%p>\n", window);
  auto ctx = (CtxGLFW*) glfwGetWindowUserPointer(window);
  auto uiev = ctx->uievent();

  ////////////////////////
  // TODO - resolve where to send input, IMGUI - or ork::lev2::ui ?
  ////////////////////////

  //ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, modifiers);

  ////////////////////////

  switch(action){
    case GLFW_PRESS:
      uiev->_eventcode = ui::EventCode::KEY_DOWN;
      break;
    case GLFW_RELEASE:
      uiev->_eventcode = ui::EventCode::KEY_UP;
      break;
    case GLFW_REPEAT:
      uiev->_eventcode = ui::EventCode::KEY_REPEAT;
      break;
  }
  //auto keyc       = _keymap.find(Qt::Key(ikeyUNI));
  uiev->miKeyCode = key;

  uiev->mbALT          = (modifiers & GLFW_MOD_ALT);
  uiev->mbCTRL         = (modifiers & GLFW_MOD_CONTROL);
  uiev->mbSHIFT        = (modifiers & GLFW_MOD_SHIFT);
  uiev->mbMETA         = (modifiers & GLFW_MOD_SUPER);

  _fire_ui_event(ctx);                    
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_mousebuttons(GLFWwindow* window, int button, int action, int modifiers){
  //printf("_glfw_callback_mousebuttons<%p>\n", window);

  ////////////////////////
  // TODO - resolve where to send input, IMGUI - or ork::lev2::ui ?
  ////////////////////////

  //ImGui_ImplGlfw_MouseButtonCallback(window, button, action, modifiers);

  ////////////////////////

  auto ctx = (CtxGLFW*) glfwGetWindowUserPointer(window);
  auto uiev = ctx->uievent();

  bool DOWN = (action == GLFW_PRESS);

  switch(button){
    case GLFW_MOUSE_BUTTON_LEFT:
      uiev->mbLeftButton = DOWN;
      ctx->_buttonState = (ctx->_buttonState&6) | int(DOWN);
      break;
    case GLFW_MOUSE_BUTTON_MIDDLE:
      uiev->mbMiddleButton = DOWN;
      ctx->_buttonState = (ctx->_buttonState&5) | (int(DOWN)<<1);
      break;
    case GLFW_MOUSE_BUTTON_RIGHT:
      uiev->mbRightButton = DOWN;
      ctx->_buttonState = (ctx->_buttonState&3) | (int(DOWN)<<2);
      break;
  }

  uiev->mbALT          = (modifiers & GLFW_MOD_ALT);
  uiev->mbCTRL         = (modifiers & GLFW_MOD_CONTROL);
  uiev->mbSHIFT        = (modifiers & GLFW_MOD_SHIFT);
  uiev->mbMETA         = (modifiers & GLFW_MOD_SUPER);


  uiev->_eventcode = DOWN //
                   ? ork::ui::EventCode::PUSH //
                   : ork::ui::EventCode::RELEASE;

  _fire_ui_event(ctx);                    

  /////////////////////////

}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_scroll(GLFWwindow* window, double xoffset, double yoffset){
  //printf("_glfw_callback_scroll<%p>\n", window);

  //ImGui_ImplGlfw_ScrollCallback(window, xoffset, yoffset);

  auto ctx = (CtxGLFW*)glfwGetWindowUserPointer(window);
  auto uiev = ctx->uievent();
  uiev->_eventcode = ui::EventCode::MOUSEWHEEL;
  uiev->miMWY = int(yoffset);
  _fire_ui_event(ctx);                    
}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_cursor(GLFWwindow* window, double xoffset, double yoffset){
  //printf("_glfw_callback_cursor<%p>\n", window);

  auto ctx = (CtxGLFW*)glfwGetWindowUserPointer(window);
  auto uiev = ctx->uievent();

  uiev->mpBlindEventData = nullptr;

  // InputManager::instance()->poll();


  //int ix = event->x();
  //int iy = event->y();
  //if (_HIDPI()) {
    //ix /= 2;
    //iy /= 2;
  //}

#if defined(__APPLE__)
  if (false and _macosUseHIDPI) {
    xoffset *= 2;
    yoffset *= 2;
  }
#endif

  uiev->miLastX = uiev->miX;
  uiev->miLastY = uiev->miY;

  uiev->miX = int(xoffset);
  uiev->miY = int(yoffset);

  float unitX = xoffset / float(ctx->_width);
  float unitY = yoffset / float(ctx->_height);

  uiev->mfLastUnitX = uiev->mfUnitX;
  uiev->mfLastUnitY = uiev->mfUnitY;
  uiev->mfUnitX     = unitX;
  uiev->mfUnitY     = unitY;

  if(ctx->_buttonState==0){
      uiev->_eventcode = ui::EventCode::MOVE; //
      _fire_ui_event(ctx);                   
  }
  else{
      uiev->_eventcode = ui::EventCode::DRAG; //
      _fire_ui_event(ctx);                   
  }


}
///////////////////////////////////////////////////////////////////////////////
static void _glfw_callback_enterleave(GLFWwindow* window, int entered) {
  //printf("_glfw_callback_enterleave<%p> entered<%d>\n", window, entered);

  //ImGui_ImplGlfw_CursorEnterCallback(window, entered);

  bool was_entered = bool(entered);
  auto ctx = (CtxGLFW*)glfwGetWindowUserPointer(window);
  auto uiev = ctx->uievent();

  uiev->_eventcode       = was_entered //
                         ? ui::EventCode::GOT_KEYFOCUS //
                         : ui::EventCode::LOST_KEYFOCUS;

  if(ctx->GetWindow()){
    if(was_entered) {
      ctx->GetWindow()->GotFocus();
    }
    else {
      ctx->GetWindow()->LostFocus();
    }
  }
  _fire_ui_event(ctx);         
}
///////////////////////////////////////////////////////////////////////////////
CtxGLFW::CtxGLFW(Window* ork_win) //, QWidget* pparent)
    : CTXBASE(ork_win) {

  _onRunLoopIteration = []() {};

  _uievent = std::make_shared<ui::Event>();

  _runstate = 1;

  //printf("CtxGLFW created<%p> glfw_win<%p> isglobal<%d>\n", this, _glfwWindow, int(isGlobal()));
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::initWithData(appinitdata_ptr_t aid) {
  _appinitdata = aid;
  glfwSwapInterval(aid->_swap_interval);
  glfwWindowHint(GLFW_SAMPLES, aid->_msaa_samples);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::Show() {

  if (_orkwindow) {
    _orkwindow->SetDirty(true);

    GLFWmonitor* fullscreen_monitor = nullptr;

    if (_appinitdata->_fullscreen) {

      fullscreen_monitor = glfwGetPrimaryMonitor();

      int l = _appinitdata->_left;
      int t = _appinitdata->_top;

      int monitor_count = 0;
      auto monitors = glfwGetMonitors (&monitor_count);

      int idiff = 100000;

      for( int i=0; i<monitor_count; i++ ){
        GLFWmonitor* monitor = monitors[i];
        int mon_x = 0;
        int mon_y = 0;
        glfwGetMonitorPos (monitor, &mon_x, &mon_y);

        /////////////////////////////////
        // select monitor whose left edge is the closest to the appinitdata's left
        /////////////////////////////////

        int d = abs(mon_x - l);

        if( d<idiff ){
          fullscreen_monitor = monitor;
          idiff = d;
          //printf( "USING MONITOR<%p> \n", fullscreen_monitor );
        }

        /////////////////////////////////
      }

      //////////////////////////////////////
      // technically "windowed fullscreen"
      //////////////////////////////////////
      const GLFWvidmode* mode = glfwGetVideoMode(fullscreen_monitor);
      glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
      _width = mode->width;
      _height = mode->height;
      //printf( "USING GLFW_REFRESH_RATE<%d> \n", int(mode->refreshRate) );
      //printf( "USING GLFW _width<%d> \n", _width );
      //printf( "USING GLFW _height<%d> \n", _height );
      //////////////////////////////////////
    }

    if( _appinitdata->_offscreen ){
      glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
      glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE );
      glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    }
    else{
      glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
      glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
      glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    }

    auto global = globalOffscreenContext();

    _glfwWindow = glfwCreateWindow(
        _width,             //
        _height,            //
        "OrkWindow",        //
        fullscreen_monitor, // monitor
        global->_glfwWindow // sharegroup
    );
    OrkAssert(_glfwWindow != nullptr);
    glfwSetWindowUserPointer(_glfwWindow, (void*)this);
    glfwSetWindowAttrib(_glfwWindow,GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);

    if( not _appinitdata->_offscreen ){
      glfwSetWindowAttrib(_glfwWindow,GLFW_FOCUS_ON_SHOW ,GLFW_TRUE);
      //glfwSetInputMode(_glfwWindow, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
      //glfwSetInputMode(_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    glfwSetWindowRefreshCallback(_glfwWindow, _glfw_callback_refresh);
    glfwSetFramebufferSizeCallback(_glfwWindow, _glfw_callback_fbresized);
    glfwSetWindowSizeCallback(_glfwWindow, _glfw_callback_winresized);
    glfwSetWindowContentScaleCallback(_glfwWindow, _glfw_callback_contentScaleChanged);
    glfwSetWindowFocusCallback(_glfwWindow, _glfw_callback_focusChanged);
    glfwSetKeyCallback(_glfwWindow, _glfw_callback_keyboard);
    glfwSetMouseButtonCallback(_glfwWindow,_glfw_callback_mousebuttons);
    glfwSetScrollCallback(_glfwWindow,_glfw_callback_scroll);
    glfwSetCursorPosCallback(_glfwWindow,_glfw_callback_cursor);
    glfwSetCursorEnterCallback(_glfwWindow, _glfw_callback_enterleave);


  }

  if (_appinitdata->_fullscreen) {

    _glfw_callback_fbresized(_glfwWindow,_width,_height);

  } else {
    glfwSetWindowPos(
        _glfwWindow,
        _appinitdata->_left, //
        _appinitdata->_top);
    glfwSetWindowSize(
        _glfwWindow,
        _appinitdata->_width, //
        _appinitdata->_height);
  }

  if (_needsInitialize) {
    //printf("CreateCONTEXT\n");
    _orkwindow->initContext();
    _orkwindow->OnShow();
    _needsInitialize = false;
  }
  if( not _appinitdata->_offscreen ){
    glfwShowWindow(_glfwWindow);
  }
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::Hide() {
  glfwHideWindow(_glfwWindow);
}
///////////////////////////////////////////////////////////////////////////////
CtxGLFW::~CtxGLFW() {
  _runstate = 2;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::makeCurrent() {
  // printf( "CtxGLFW makeCurrent<%p> glfw_win<%p> isglobal<%d>\n", this, _glfwWindow, int(isGlobal()) );
  glfwMakeContextCurrent(_glfwWindow);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::swapBuffers() {
  glfwSwapBuffers(_glfwWindow);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::SetAlwaysRun(bool brun) {
  mbAlwaysRun = brun;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::signalExit(){
  _runstate = 2;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::pollEvents(){
  glfwPollEvents();
}
///////////////////////////////////////////////////////////////////////////////
int CtxGLFW::runloop() {
  int rval = 0;

  OrkAssert(_target);

  lev2::ThreadGfxContext l2ctx_track(_target);

  _target->makeCurrentContext();

  if(_onGpuInit){
    _onGpuInit(_target);
  }

  while (_runstate == 1) {

    //////////////////////////////
    // poll UI/windowing system events
    //////////////////////////////

    //glfwWaitEvents();
    glfwPollEvents();

    //////////////////////////////
    // run main thread app logic
    //////////////////////////////

    _onRunLoopIteration();

    //////////////////////////////
    // redraw ?
    //////////////////////////////

    auto policy_item = currentRefreshPolicy();
    auto policy      = policy_item._policy;
    // if( policy == EREFRESH_FASTEST)
    SlotRepaint();


    //////////////////////////////
    // check for closed window
    //////////////////////////////

    if (_glfwWindow) {
      bool window_should_close = glfwWindowShouldClose(_glfwWindow);
      if (window_should_close) {
        _runstate = 2;
      }
    }

  } //  while (_runstate == 1) {

  //////////////////////////////
  // run main thread app logic (one last time..)
  //////////////////////////////

  //_onRunLoopIteration();

  //////////////////////////////

  if(_onGpuExit){
    _onGpuExit(_target);
  }

  //////////////////////////////

  glfwDestroyWindow(_glfwWindow);
  _runstate = 3;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
fvec2 CtxGLFW::MapCoordToGlobal(const fvec2& v) const {
  return v; // QPoint p(v.x, v.y);
  // QPoint p2 = mpQtWidget->mapToGlobal(p);
  // return fvec2(p2.x(), p2.y());
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::onResize(int W, int H) {
  _uievent->mpBlindEventData = nullptr;
  //////////////////////////////////////////////////////////
  lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
  //////////////////////////////////////////////////////////

  if (_target) {
    _target->resizeMainSurface(W, H);
    _uievent->mpGfxWin = (Window*)_target->FBI()->GetThisBuffer();
    if (_uievent->mpGfxWin)
      _uievent->mpGfxWin->Resize(0, 0, W, H);
  }
  lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();

  _width  = W;
  _height = H;
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::_doEnqueueWindowResize( int w, int h ) {
  auto op = [=](){
    glfwSetWindowSize(_glfwWindow, w, h);
  };
  opq::mainSerialQueue()->enqueue(op);
}
///////////////////////////////////////////////////////////////////////////////
void CtxGLFW::SlotRepaint() {
  OrkAssert(opq::TrackCurrent::is(opq::mainSerialQueue()));

  //auto lamb = [&]() {
    if (not GfxEnv::initialized())
      return;

    ork::PerfMarkerPush("ork.viewport.draw.begin");

    this->mDrawLock++;
    if (this->mDrawLock == 1) {
      // printf( "CtxGLFW::SlotRepaint() _target<%p>\n", _target );
      if (this->_target) {
        _target->makeCurrentContext();
        auto gfxwin = _uievent->mpGfxWin;
        auto vp     = gfxwin ? gfxwin->GetRootWidget() : nullptr;

        _uievent->mpGfxWin = (Window*) _target->FBI()->GetThisBuffer();
        auto drwev = std::make_shared<ui::DrawEvent>(this->_target);

        if (vp)
          vp->Draw(drwev);
      }
    }
    this->mDrawLock--;
    ork::PerfMarkerPush("ork.viewport.draw.end");
    //glFinish();
  //};

  //lamb();
}
///////////////////////////////////////////////////////////////////////////////
void error_callback( int error, const char *msg ) {
    printf( "GLFW ERROR<%d:%s>\n", error, msg );
}
CtxGLFW* CtxGLFW::globalOffscreenContext() {
  static CtxGLFW* gctx = nullptr;
  if (nullptr == gctx) {

    glfwSetErrorCallback( error_callback );

    gctx = new CtxGLFW(nullptr);

    bool ok = glfwInit();
    assert(ok);

    auto primary_monitor = glfwGetPrimaryMonitor();
    if(primary_monitor){
      const GLFWvidmode* mode = glfwGetVideoMode(primary_monitor);

      //printf( "primary_monitor<%p>\n", primary_monitor );
      //printf( "mode<%p>\n", mode );
      //printf( "mode redbits<%d>\n", mode->redBits );
      //printf( "mode grnbits<%d>\n", mode->greenBits );
      //printf( "mode blubits<%d>\n", mode->blueBits );
      //printf( "mode refreshRate<%d>\n", mode->refreshRate );
      //glfwWindowHint(GLFW_RED_BITS, mode->redBits);
      //glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
      //glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
      //glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);

    }

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 4 );

#if defined(OPENGL_46)
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 6 );
#elif defined(OPENGL_41)
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 1 );
#elif defined(OPENGL_40)
    glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 0 );
#endif

    glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE );
    glfwWindowHint( GLFW_CLIENT_API, GLFW_OPENGL_API );
    glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

    // this can fail on nvidia aarch64 devices
    //  see: https://github.com/isl-org/Open3D/issues/2549

    auto offscreen_window = glfwCreateWindow(
        32,     //
        32,     //
        "",      //
        nullptr, //
        nullptr);
    //printf( "offscreen_window<%p>\n", offscreen_window );
    OrkAssert(offscreen_window!=nullptr);
    gctx->_glfwWindow = offscreen_window;
    glfwSetWindowAttrib(offscreen_window,GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    glfwSetWindowUserPointer(offscreen_window, (void*)gctx);
    OrkAssert(offscreen_window);
    glfwSwapInterval(0);
  }
  return gctx;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
