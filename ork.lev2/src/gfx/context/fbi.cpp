////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

std::function<void(Context*)> FrameBufferInterface::_hackcb = nullptr;

FrameBufferInterface::FrameBufferInterface(Context& tgt)
    : _target(tgt)
    , _enableVSync(false)
    , _enableFullScreen(GfxEnv::GetRef().GetCreationParams().mbFullScreen)
    , _autoClear(true)
    , miViewportStackIndex(0)
    , miScissorStackIndex(0)
    , maScissorStack(kiVPStackMax)
    , maViewportStack(kiVPStackMax)
    , _clearColor(fcolor4::Black())
    , _pickState(0) {

  // for( int i=0; i<kiVPStackMax; i++ )
  //	maViewportStack[i]
}

///////////////////////////////////////////////////////////////////////////////

FrameBufferInterface::~FrameBufferInterface() {
}

///////////////////////////////////////////////////////////////////////////////

bool FrameBufferInterface::capture(const RtBuffer* rtb, CaptureBuffer* capbuf) {
  auto rtb_format = rtb->format();
  return captureAsFormat(rtb, capbuf, rtb_format);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::EnterPickState(PickBuffer* pb) {
  _pickState++;

  // printf("enter _pickState<%d>\n", _pickState);

  _pickbuffer = pb;
}
bool FrameBufferInterface::isPickState() const {
  return (_pickState > 0);
}

void FrameBufferInterface::LeavePickState() {
  _pickState--;
  // printf("leave _pickState<%d>\n", _pickState);
  OrkAssert(_pickState >= 0);
  _pickbuffer = 0;
}
PickBuffer* FrameBufferInterface::currentPickBuffer() const {
  return _pickbuffer;
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::PushRtGroup(RtGroup* Base) {

  bool first = mRtGroupStack.empty();

  mRtGroupStack.push(_currentRtGroup);
  SetRtGroup(Base);

  int iw = _target.mainSurfaceWidth();
  int ih = _target.mainSurfaceHeight();

  if (Base != nullptr) {
    iw = Base->width();
    ih = Base->height();
  }

  ViewportRect r(0, 0, iw, ih);

  pushScissor(r);
  pushViewport(r);

  if (Base->_autoclear) {
    rtGroupClear(Base);
  }
  // BeginFrame();
}
void FrameBufferInterface::PopRtGroup() {
  RtGroup* prev = mRtGroupStack.top();
  mRtGroupStack.pop();
  // EndFrame();
  SetRtGroup(prev); // Enable Mrt
  popViewport();
  popScissor();
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::BeginFrame(void) {
  _doBeginFrame();
  if (_hackcb)
    _hackcb(&_target);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::EndFrame(void) {
  _doEndFrame();
}

///////////////////////////////////////////////////////////////////////////////

ViewportRect::ViewportRect()
    : ui::Rect() {
}
ViewportRect::ViewportRect(int x, int y, int w, int h)
    : ui::Rect(0, 0, 0, 0) {
  _x = OldStlSchoolClampToRange(x, 0, 16384);
  _y = OldStlSchoolClampToRange(y, 0, 16384);
  _w = OldStlSchoolClampToRange(w, 8, 16384);
  _h = OldStlSchoolClampToRange(h, 8, 16384);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::pushScissor(const ViewportRect& rect) {
  maScissorStack[++miScissorStackIndex] = rect;
  _setScissor(rect._x, rect._y, rect._w, rect._h);
}
void FrameBufferInterface::pushViewport(const ViewportRect& rect) {
  int oldlev                              = miViewportStackIndex;
  auto oldvp                              = maViewportStack[miViewportStackIndex];
  maViewportStack[++miViewportStackIndex] = rect;
  _setViewport(rect._x, rect._y, rect._w, rect._h);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::pushScissor(int x, int y, int w, int h) {
  OrkAssert((miScissorStackIndex + 1) < kiVPStackMax);
  ViewportRect rect(x, y, w, h);
  maScissorStack[++miScissorStackIndex] = rect;
  _setScissor(x, y, w, h);
}
void FrameBufferInterface::pushViewport(int x, int y, int w, int h) {
  OrkAssert((miViewportStackIndex + 1) < kiVPStackMax);
  ViewportRect rect(x, y, w, h);
  maViewportStack[++miViewportStackIndex] = rect;
  _setViewport(x, y, w, h);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::setScissor(const ViewportRect& rect) {
  maViewportStack[miScissorStackIndex] = rect;
  _setScissor(rect._x, rect._y, rect._w, rect._h);
}
void FrameBufferInterface::setViewport(const ViewportRect& rect) {
  maViewportStack[miViewportStackIndex] = rect;
  _setViewport(rect._x, rect._y, rect._w, rect._h);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::popViewport() {
  int oldlev = miViewportStackIndex;
  auto oldvp = maViewportStack[miViewportStackIndex];
  OrkAssert(miViewportStackIndex > 0);
  ViewportRect& rect = maViewportStack[--miViewportStackIndex];
  _setViewport(rect._x, rect._y, rect._w, rect._h);
}
void FrameBufferInterface::popScissor() {
  OrkAssert(miScissorStackIndex > 0);
  ViewportRect& rect = maScissorStack[--miScissorStackIndex];
  _setScissor(rect._x, rect._y, rect._w, rect._h);
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

const ViewportRect& FrameBufferInterface::viewport() const {
  return maViewportStack[miViewportStackIndex];
}

const ViewportRect& FrameBufferInterface::scissor() const {
  OrkAssert(miScissorStackIndex >= 0);
  OrkAssert(miScissorStackIndex < kiVPStackMax);
  return maScissorStack[miScissorStackIndex];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
