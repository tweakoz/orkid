////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

std::function<void(Context*)> FrameBufferInterface::_hackcb = nullptr;

FrameBufferInterface::FrameBufferInterface(Context& tgt)
    : mTarget(tgt)
    , mbEnableVSync(false)
    , mbEnableFullScreen(GfxEnv::GetRef().GetCreationParams().mbFullScreen)
    , mpBufferTex(0)
    , mbAutoClear(true)
    , _clearColor(fcolor4::Black())
    , mpThisBuffer(0)
    , miScissorStackIndex(0)
    , miViewportStackIndex(0)
    , maScissorStack(kiVPStackMax)
    , maViewportStack(kiVPStackMax)
    , mCurrentRtGroup(0)
    , miPickState(0)
    , _pickbuffer(0) {
  // for( int i=0; i<kiVPStackMax; i++ )
  //	maViewportStack[i]
}

///////////////////////////////////////////////////////////////////////////////

FrameBufferInterface::~FrameBufferInterface() {
  // if( mpBufferTex )
  //{
  //	delete mpBufferTex;
  //}
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::EnterPickState(PickBuffer* pb) {
  miPickState++;

  printf("enter miPickState<%d>\n", miPickState);

  _pickbuffer = pb;
}
bool FrameBufferInterface::isPickState() const {
  return (miPickState > 0);
}

void FrameBufferInterface::LeavePickState() {
  miPickState--;
  printf("leave miPickState<%d>\n", miPickState);
  OrkAssert(miPickState >= 0);
  _pickbuffer = 0;
}
PickBuffer* FrameBufferInterface::currentPickBuffer() const {
  return _pickbuffer;
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::PushRtGroup(RtGroup* Base) {

  bool first = mRtGroupStack.empty();

  mRtGroupStack.push(mCurrentRtGroup);
  SetRtGroup(Base);

  int iw = mTarget.mainSurfaceWidth();
  int ih = mTarget.mainSurfaceHeight();

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
    _hackcb(&mTarget);
}

///////////////////////////////////////////////////////////////////////////////

void FrameBufferInterface::EndFrame(void) {
  _doEndFrame();
}

///////////////////////////////////////////////////////////////////////////////

ViewportRect::ViewportRect(int x, int y, int w, int h) {
  _x = OldStlSchoolClampToRange(x, 0, 16384);
  _y = OldStlSchoolClampToRange(y, 0, 16384);
  _w = OldStlSchoolClampToRange(w, 32, 16384);
  _h = OldStlSchoolClampToRange(h, 32, 16384);
}
ViewportRect::ViewportRect() {
  _x = 0;
  _y = 0;
  _w = 32;
  _h = 32;
}
SRect ViewportRect::asSRect() const {
  SRect rval(_x, _y, _w, _h);
  return rval;
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
