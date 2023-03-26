////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////
/// Raster State Interface
/// ////////////////////////////////////////////////////////////////////////////
/// ////////////////////////////////////////////////////////////////////////////

struct RGBAMask {
  bool _r = true;
  bool _g = true;
  bool _b = true;
  bool _a = true;
};

class RasterStateInterface {
public:
  RasterStateInterface(Context& target);

  SRasterState& GetRasterState(void) { return mCurrentState; }
  SRasterState& RefUIRasterState(void) { return mUIRasterState; }
  virtual void BindRasterState(const SRasterState& rState, bool bForce = false) = 0;

  SRasterState& GetLastState(void) { return mLastState; }

  virtual void SetZWriteMask(bool bv)             = 0;
  virtual void SetRGBAWriteMask(bool rgb, bool a) = 0;
  virtual RGBAMask SetRGBAWriteMask(const RGBAMask& newmask) = 0;
  virtual void SetBlending(Blending eVal)        = 0;
  virtual void SetDepthTest(EDepthTest eVal)      = 0;
  virtual void SetCullTest(ECullTest eVal)        = 0;
  virtual void setScissorTest(EScissorTest eVal)  = 0;

protected:
  Context& _target;
  SRasterState mUIRasterState;
  SRasterState mCurrentState;
  SRasterState mLastState;
  RGBAMask _curmask;
};
