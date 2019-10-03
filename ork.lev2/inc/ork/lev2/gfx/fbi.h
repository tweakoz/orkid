#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Frame/Buffer / Control Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class FrameBufferInterface {
public:
  FrameBufferInterface(GfxTarget& mTarget);
  ~FrameBufferInterface();

  Texture* GetBufferTexture(void) { return mpBufferTex; }
  void SetBufferTexture(Texture* ptex) { mpBufferTex = ptex; }
  void SetClearColor(const fcolor4& scol) { mcClearColor = scol; }
  const fcolor4& GetClearColor() const { return mcClearColor; }
  void SetAutoClear(bool bv) { mbAutoClear = bv; }
  bool GetAutoClear() const { return mbAutoClear; }
  void SetVSyncEnable(bool bv) { mbEnableVSync = bv; }
  GfxBuffer* GetThisBuffer(void) { return mpThisBuffer; }
  void SetThisBuffer(GfxBuffer* pbuf) { mpThisBuffer = pbuf; }
  bool IsOffscreenTarget(void) { return mbIsPbuffer; }
  void SetOffscreenTarget(bool bv) { mbIsPbuffer = bv; }
  virtual void SetRtGroup(RtGroup* Base) = 0;
  RtGroup* GetRtGroup() const { return mCurrentRtGroup; }

  void PushRtGroup(RtGroup* Base);
  void PopRtGroup();

  ///////////////////////////////////////////////////////

  virtual void SetViewport(int iX, int iY, int iW, int iH) = 0;
  virtual void SetScissor(int iX, int iY, int iW, int iH)  = 0;
  virtual void Clear(const fcolor4& rCol, float fdepth)    = 0;
  virtual void PushViewport(const SRect& rViewportRect);
  virtual SRect& PopViewport(void);
  SRect& GetViewport(void);
  int GetVPX(void) { return miCurVPX; }
  int GetVPY(void) { return miCurVPY; }
  int GetVPW(void) { return miCurVPW; }
  int GetVPH(void) { return miCurVPH; }

  ///////////////////////////////////////////////////////

  virtual void PushScissor(const SRect& rScissorRect);
  virtual SRect& PopScissor(void);
  inline SRect& GetScissor(void);

  //////////////////////////////////////////////
  // Capture Interface

  virtual void Capture(const RtGroup& inpbuf, int irt, const file::Path& pth) {}
  virtual bool CaptureToTexture(const CaptureBuffer& capbuf, Texture& tex) { return false; }
  virtual void GetPixel(const fvec4& rAt, PixelFetchContext& ctx) = 0;

  //////////////////////////////////////////////

  void BeginFrame(void);
  void EndFrame(void);
  virtual void DoBeginFrame(void) = 0;
  virtual void DoEndFrame(void)   = 0;

  //////////////////////////////////////////////

  void EnterPickState(PickBufferBase* pb) {
    miPickState++;
    mpPickBuffer = pb;
  }
  bool IsPickState(void) { return (miPickState > 0); }

  void LeavePickState(void) {
    miPickState--;
    OrkAssert(miPickState >= 0);
    mpPickBuffer = 0;
  }

  PickBufferBase* GetCurrentPickBuffer() const { return mpPickBuffer; }

  //////////////////////////////////////////////

protected:
  static const int kiVPStackMax = 16;

  int miViewportStackIndex;
  int miScissorStackIndex;
  SRect maScissorStack[kiVPStackMax];
  SRect maViewportStack[kiVPStackMax];

  GfxBuffer* mpThisBuffer;
  Texture* mpBufferTex;
  RtGroup* mCurrentRtGroup;
  fcolor4 mcClearColor;
  bool mbAutoClear;
  bool mbIsPbuffer;
  bool mbEnableFullScreen;
  bool mbEnableVSync;
  int miCurVPX;
  int miCurVPY;
  int miCurVPW;
  int miCurVPH;
  int miPickState;
  GfxTarget& mTarget;
  PickBufferBase* mpPickBuffer;
  std::stack<lev2::RtGroup*> mRtGroupStack;
};
