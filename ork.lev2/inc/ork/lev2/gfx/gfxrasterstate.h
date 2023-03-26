////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv_enum.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

struct SRasterState {
  // Render States

  unsigned muZWriteMask : 1;   
  unsigned muAWriteMask : 1;   
  unsigned muRGBWriteMask : 1; 

  unsigned muAlphaTest : 2;
  unsigned muAlphaRef : 4; 

  unsigned muScissorTest : 1; 
  unsigned muShadeModel : 1; 
  unsigned muStencilMode : 4; 
  unsigned muStencilRef : 8;  
  unsigned muStencilMask : 3; 
  unsigned muStencilOpPass : 3;
  unsigned muStencilOpFail : 3;

  // Sort (Highest Priority)

  unsigned muSortID : 11;     
  unsigned muTransparent : 1; 

  float mPointSize;

  /////////////////////////////

  ECullTest _culltest = ECullTest::OFF;         
  EDepthTest _depthtest  = EDepthTest::LEQUALS; 
  Blending _blending  = Blending::OFF; 

  /////////////////////////////
  // Accessors

  void SetRGBAWriteMask(bool rgb, bool a) {
    muRGBWriteMask = (unsigned)rgb;
    muAWriteMask   = (unsigned)a;
  }
  void SetZWriteMask(bool bv) {
    muZWriteMask = (unsigned)bv;
  }

  bool GetZWriteMask(void) const {
    return (bool)muZWriteMask;
  }
  bool GetAWriteMask(void) const {
    return (bool)muAWriteMask;
  }
  bool GetRGBWriteMask(void) const {
    return (bool)muRGBWriteMask;
  }

  /////////////////////////////

  void setScissorTest(EScissorTest eVal) {
    muScissorTest = eVal;
  }
  void SetAlphaTest(EAlphaTest eVal, f32 falpharef = 0.0f) {
    muAlphaTest = eVal;
    muAlphaRef  = (unsigned)(falpharef * 16.0f);
  }
  void SetBlending(Blending eVal) {
    _blending = eVal;
  }
  void SetDepthTest(EDepthTest eVal) {
    _depthtest = eVal;
  }
  void SetShadeModel(EShadeModel eVal) {
    muShadeModel = eVal;
  }
  void SetCullTest(ECullTest eVal) {
    _culltest = eVal;
  }
  void SetStencilMode(EStencilMode eVal, EStencilOp ePassOp, EStencilOp eFailOp, u8 uRef, u8 uMsk) {
    muStencilOpPass = (unsigned)ePassOp;
    muStencilOpFail = (unsigned)eFailOp;
    muStencilMode   = (unsigned)eVal;
    muStencilRef    = uRef;
    muStencilMask   = uMsk;
  }

  EScissorTest GetScissorTest(void) const {
    return EScissorTest(muScissorTest);
  }
  EAlphaTest GetAlphaTest(void) const {
    return EAlphaTest(muAlphaTest);
  }
  Blending GetBlending(void) const {
    return _blending;
  }
  EDepthTest GetDepthTest(void) const {
    return _depthtest;
  }
  EShadeModel GetShadeModel(void) const {
    return EShadeModel(muShadeModel);
  }
  ECullTest GetCullTest(void) const {
    return _culltest;
  }
  void GetStencilMode(EStencilMode& eVal, EStencilOp& ePassOp, EStencilOp& eFailOp, u8& uRef, u8& uMsk) const {
    ePassOp = (EStencilOp)muStencilOpPass;
    eFailOp = (EStencilOp)muStencilOpFail;
    eVal    = (EStencilMode)muStencilMode;
    uRef    = muStencilRef;
    uMsk    = muStencilMask;
  }

  /////////////////////////////

  void SetSortID(unsigned int uVal) {
    muSortID = uVal;
  }
  void SetTransparent(bool bVal) {
    muTransparent = bVal;
  }

  unsigned int GetSortID(void) const {
    return (unsigned int)(muSortID);
  }
  bool GetTransparent(void) const {
    return bool(muTransparent);
  }

  void SetPointSize(float i) {
    mPointSize = i;
  }
  float GetPointSize() const {
    return mPointSize;
  }

  /////////////////////////////////////////////////////////////////////////

  SRasterState();
};

}} // namespace ork::lev2
