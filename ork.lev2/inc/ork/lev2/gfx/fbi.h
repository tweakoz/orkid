#pragma once

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
    mbAutoClear = bv;
  }
  bool GetAutoClear() const {
    return mbAutoClear;
  }
  void SetVSyncEnable(bool bv) {
    mbEnableVSync = bv;
  }

  ///////////////////////////////////////////////////////

  Texture* GetBufferTexture() {
    return mpBufferTex;
  }
  void SetBufferTexture(Texture* ptex) {
    mpBufferTex = ptex;
  }

  ///////////////////////////////////////////////////////

  OffscreenBuffer* GetThisBuffer() {
    return mpThisBuffer;
  }
  void SetThisBuffer(OffscreenBuffer* pbuf) {
    mpThisBuffer = pbuf;
  }

  ///////////////////////////////////////////////////////

  virtual void SetRtGroup(RtGroup* Base) = 0;
  RtGroup* GetRtGroup() const {
    return mCurrentRtGroup;
  }

  void PushRtGroup(RtGroup* Base);
  void PopRtGroup();

  virtual void rtGroupClear(RtGroup* rtg) {
  }
  virtual void rtGroupMipGen(RtGroup* rtg) {
  }

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

  virtual void _setViewport(int iX, int iY, int iW, int iH) = 0;
  virtual void _setScissor(int iX, int iY, int iW, int iH)  = 0;
  virtual void Clear(const fcolor4& rCol, float fdepth)     = 0;
  virtual void clearDepth(float fdepth)                     = 0;

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

  ///////////////////////////////////////////////////////
  // Capture Interface
  ///////////////////////////////////////////////////////

  virtual bool capture(const RtGroup& inpbuf, int irt, CaptureBuffer* buffer) {
    return false;
  }
  virtual bool captureAsFormat(const RtGroup& inpbuf, int irt, CaptureBuffer* buffer, EBufferFormat destfmt) {
    return false;
  }
  virtual void Capture(const RtGroup& inpbuf, int irt, const file::Path& pth) {
  }
  virtual bool CaptureToTexture(const CaptureBuffer& capbuf, Texture& tex) {
    return false;
  }
  virtual void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) = 0;

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

  int miViewportStackIndex;
  int miScissorStackIndex;
  ork::fixedvector<ViewportRect, kiVPStackMax> maScissorStack;
  ork::fixedvector<ViewportRect, kiVPStackMax> maViewportStack;

  OffscreenBuffer* mpThisBuffer;
  Texture* mpBufferTex;
  RtGroup* mCurrentRtGroup;
  fcolor4 _clearColor;
  bool mbAutoClear;
  bool mbEnableFullScreen;
  bool mbEnableVSync;
  int miPickState;
  Context& mTarget;
  PickBuffer* _pickbuffer;
  std::stack<lev2::RtGroup*> mRtGroupStack;
};
