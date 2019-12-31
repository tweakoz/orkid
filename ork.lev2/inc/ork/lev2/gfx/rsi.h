#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Raster State Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

class RasterStateInterface {
public:
  RasterStateInterface(Context& target);

  SRasterState& GetRasterState(void) { return mCurrentState; }
  SRasterState& RefUIRasterState(void) { return mUIRasterState; }
  virtual void BindRasterState(const SRasterState& rState, bool bForce = false) = 0;

  SRasterState& GetLastState(void) { return mLastState; }

  virtual void SetZWriteMask(bool bv)             = 0;
  virtual void SetRGBAWriteMask(bool rgb, bool a) = 0;
  virtual void SetBlending(EBlending eVal)        = 0;
  virtual void SetDepthTest(EDepthTest eVal)      = 0;
  virtual void SetCullTest(ECullTest eVal)        = 0;
  virtual void SetScissorTest(EScissorTest eVal)  = 0;

protected:
  Context& _target;
  SRasterState mUIRasterState;
  SRasterState mCurrentState;
  SRasterState mLastState;
};
