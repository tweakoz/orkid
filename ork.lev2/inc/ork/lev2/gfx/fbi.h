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

  Texture* GetBufferTexture(void) {
    return mpBufferTex;
  }
  void SetBufferTexture(Texture* ptex) {
    mpBufferTex = ptex;
  }
  void SetClearColor(const fcolor4& scol) {
    mcClearColor = scol;
  }
  const fcolor4& GetClearColor() const {
    return mcClearColor;
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
  OffscreenBuffer* GetThisBuffer(void) {
    return mpThisBuffer;
  }
  void SetThisBuffer(OffscreenBuffer* pbuf) {
    mpThisBuffer = pbuf;
  }
  virtual void SetRtGroup(RtGroup* Base) = 0;
  RtGroup* GetRtGroup() const {
    return mCurrentRtGroup;
  }

  void PushRtGroup(RtGroup* Base);
  void PopRtGroup();

  ///////////////////////////////////////////////////////

  virtual void SetViewport(int iX, int iY, int iW, int iH) = 0;
  virtual void SetScissor(int iX, int iY, int iW, int iH)  = 0;
  virtual void Clear(const fcolor4& rCol, float fdepth)    = 0;
  virtual void clearDepth(float fdepth)                    = 0;
  virtual void PushViewport(const SRect& rViewportRect);
  virtual SRect& PopViewport(void);
  SRect& GetViewport(void);
  int GetVPX(void) {
    return miCurVPX;
  }
  int GetVPY(void) {
    return miCurVPY;
  }
  int GetVPW(void) {
    return miCurVPW;
  }
  int GetVPH(void) {
    return miCurVPH;
  }

  ///////////////////////////////////////////////////////

  virtual void PushScissor(const SRect& rScissorRect);
  virtual SRect& PopScissor(void);
  inline SRect& GetScissor(void);

  //////////////////////////////////////////////
  // Capture Interface

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

  void BeginFrame(void);
  void EndFrame(void);
  virtual void _doBeginFrame(void) = 0;
  virtual void _doEndFrame(void)   = 0;

  //////////////////////////////////////////////

  void EnterPickState(PickBufferBase* pb) {
    miPickState++;
    mpPickBuffer = pb;
  }
  bool isPickState(void) {
    return (miPickState > 0);
  }

  void LeavePickState(void) {
    miPickState--;
    OrkAssert(miPickState >= 0);
    mpPickBuffer = 0;
  }

  PickBufferBase* GetCurrentPickBuffer() const {
    return mpPickBuffer;
  }

  //////////////////////////////////////////////

  static std::function<void(Context*)> _hackcb;

protected:
  static const int kiVPStackMax = 16;

  int miViewportStackIndex;
  int miScissorStackIndex;
  SRect maScissorStack[kiVPStackMax];
  SRect maViewportStack[kiVPStackMax];

  OffscreenBuffer* mpThisBuffer;
  Texture* mpBufferTex;
  RtGroup* mCurrentRtGroup;
  fcolor4 mcClearColor;
  bool mbAutoClear;
  bool mbEnableFullScreen;
  bool mbEnableVSync;
  int miCurVPX;
  int miCurVPY;
  int miCurVPW;
  int miCurVPH;
  int miPickState;
  Context& mTarget;
  PickBufferBase* mpPickBuffer;
  std::stack<lev2::RtGroup*> mRtGroupStack;
};
