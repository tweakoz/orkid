////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::lev2{

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Frame/Buffer / Control Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class FrameBufferInterface {
public:
  FrameBufferInterface(Context& mTarget);
  ~FrameBufferInterface();

  ///////////////////////////////////////////////////////

  void SetClearColor(const fcolor4& scol) {
    _clearColor = scol;
  }
  const fcolor4& GetClearColor() const {
    return _clearColor;
  }
  void SetAutoClear(bool bv) {
    _autoClear = bv;
  }
  bool GetAutoClear() const {
    return _autoClear;
  }
  void SetVSyncEnable(bool bv) {
    _enableVSync = bv;
  }
  
  ///////////////////////////////////////////////////////

  Texture* GetBufferTexture() {
    return _bufferTex;
  }
  void SetBufferTexture(Texture* ptex) {
    _bufferTex = ptex;
  }

  ///////////////////////////////////////////////////////

  DisplayBuffer* GetThisBuffer() {
    return _thisBuffer;
  }
  void SetThisBuffer(DisplayBuffer* pbuf) {
    _thisBuffer = pbuf;
  }

  ///////////////////////////////////////////////////////

  virtual void _pushRtGroup(rtgroup_ptr_t Base) = 0;
  virtual rtgroup_ptr_t _popRtGroup() = 0;

  void PushRtGroup(rtgroup_ptr_t Base);
  void PopRtGroup();

  virtual void rtGroupClear(rtgroup_ptr_t rtg) {
  }
  virtual void rtGroupMipGen(rtgroup_ptr_t rtg) {
  }
  //void pushMainSurface();
  //void popMainSurface();
  //virtual void _doPushMainSurface() {}
  //virtual void _doPopMainSurface() {}

  ///////////////////////////////////////////////////////
  // viewport / scissor
  ///////////////////////////////////////////////////////

  void pushViewport(int iX, int iY, int iW, int iH);
  void pushViewport(const ViewportRect& rViewportRect);
  void setViewport(const ViewportRect& rScissorRect);
  void pushScissor(int iX, int iY, int iW, int iH);
  void pushScissor(const ViewportRect& rScissorRect);
  void setScissor(const ViewportRect& rScissorRect);
  void popViewport();
  void popScissor();
  const ViewportRect& scissor() const;
  const ViewportRect& viewport() const;

  virtual void _setViewport(int iX, int iY, int iW, int iH)   = 0;
  virtual void _setScissor(int iX, int iY, int iW, int iH)    = 0;
  virtual void _clearColorAndDepth(const fcolor4& rCol, float fdepth)       = 0;
  virtual void _clearDepth(float fdepth)                       = 0;
  virtual void msaaBlit(rtgroup_ptr_t src, rtgroup_ptr_t dst) = 0;
  virtual void blit(rtgroup_ptr_t src, rtgroup_ptr_t dst) {}
  virtual void downsample2x2(rtgroup_ptr_t src, rtgroup_ptr_t dst) {}

  int GetVPX() {
    return viewport()._x;
  }
  int GetVPY() {
    return viewport()._y;
  }
  int GetVPW() {
    return viewport()._w;
  }
  int GetVPH() {
    return viewport()._h;
  }
  float currentAspectRatio() {
    return float(GetVPW())/float(GetVPH());
  }

  ///////////////////////////////////////////////////////
  // Capture Interface
  ///////////////////////////////////////////////////////

  virtual bool captureAsFormat(const RtBuffer* inpbuf, CaptureBuffer* buffer, EBufferFormat destfmt) {
    return false;
  }
  virtual void capture(const RtBuffer* inpbuf, const file::Path& pth) {
  }
  virtual bool captureToTexture(const CaptureBuffer& capbuf, Texture& tex) {
    return false;
  }
  virtual void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) = 0;

  bool capture(const RtBuffer* rtb, CaptureBuffer* capbuf);

  //////////////////////////////////////////////

  void BeginFrame();
  void EndFrame();
  virtual void _doBeginFrame() = 0;
  virtual void _doEndFrame()   = 0;

  //////////////////////////////////////////////

  void EnterPickState(PickBuffer* pb);
  bool isPickState() const;
  void LeavePickState();
  PickBuffer* currentPickBuffer() const;

  //////////////////////////////////////////////

  static std::function<void(Context*)> _hackcb;

  static const int kiVPStackMax = 16;

  Context& _target;
  Texture* _bufferTex          = nullptr;
  DisplayBuffer* _thisBuffer = nullptr;
  PickBuffer* _pickbuffer      = nullptr;
  
  bool _enableVSync;
  bool _enableFullScreen;
  bool _autoClear;

  int miViewportStackIndex;
  int miScissorStackIndex;
  ork::fixedvector<ViewportRect, kiVPStackMax> maScissorStack;
  ork::fixedvector<ViewportRect, kiVPStackMax> maViewportStack;

  fcolor4 _clearColor;
  int _pickState;
  std::stack<lev2::rtgroup_ptr_t> mRtGroupStack;
  rtgroup_ptr_t _active_rtgroup = nullptr;
  rtgroup_ptr_t _main_rtg;

  //rtbuffer_ptr_t _main_rtb_color;
  //rtbuffer_ptr_t _main_rtb_depth;

};

} //namespace ork::lev2{

