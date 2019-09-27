////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/crc.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "renderer_enum.h"
#include "compositor.h"

namespace ork::lev2 {

class CameraData;
class CameraMatrices;
class UiCamera;
class IRenderer;
class Texture;
struct LightingGroup;
class LightManager;
class GfxTarget;
class GfxBuffer;
class RtGroup;
class GfxWindow;
class XgmMaterialStateInst;
class IRenderableDag;
class IRenderTarget;
class DrawableBuffer;

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per draw instance
//  ie, per renderable (or even finer grained than that)
///////////////////////////////////////////////////////////////////////////////

struct RenderContextInstData {

  static const int kMaxEngineParamFloats = 4;
  static const int kmaxdirlights         = 4;
  static const int kmaxpntlights         = 4;

  static const RenderContextInstData Default;

  RenderContextInstData();

  //////////////////////////////////////
  // renderer interface
  //////////////////////////////////////

  void SetRenderer(const IRenderer* rnd) { mpActiveRenderer = rnd; }
  void SetDagRenderable(const IRenderableDag* rnd) { mpDagRenderable = rnd; }
  const IRenderer* GetRenderer(void) const { return mpActiveRenderer; }
  const IRenderableDag* GetDagRenderable(void) const { return mpDagRenderable; }
  const XgmMaterialStateInst* GetMaterialInst() const { return mMaterialInst; }

  void SetEngineParamFloat(int idx, float fv);
  float GetEngineParamFloat(int idx) const;

  //////////////////////////////////////
  // material interface
  //////////////////////////////////////

  int GetMaterialIndex(void) const { return miMaterialIndex; }
  int GetMaterialPassIndex(void) const { return miMaterialPassIndex; }
  void SetMaterialIndex(int idx) { miMaterialIndex = idx; }
  void SetMaterialPassIndex(int idx) { miMaterialPassIndex = idx; }
  void SetMaterialInst(const XgmMaterialStateInst* mi) { mMaterialInst = mi; }
  bool IsSkinned() const { return mbIsSkinned; }
  void SetSkinned(bool bv) { mbIsSkinned = bv; }
  void SetVertexLit(bool bv) { mbVertexLit = bv; }
  void ForceNoZWrite(bool bv) { mbForzeNoZWrite = bv; }
  bool IsForceNoZWrite() const { return mbForzeNoZWrite; }

  void SetRenderGroupState(RenderGroupState rgs) { mRenderGroupState = rgs; }
  RenderGroupState GetRenderGroupState() const { return mRenderGroupState; }

  //////////////////////////////////////
  // environment interface
  //////////////////////////////////////

  void SetTopEnvMap(Texture* ptex) { mDPTopEnvMap = ptex; }
  void SetBotEnvMap(Texture* ptex) { mDPBotEnvMap = ptex; }
  Texture* GetTopEnvMap() const { return mDPTopEnvMap; }
  Texture* GetBotEnvMap() const { return mDPBotEnvMap; }

  //////////////////////////////////////
  // lighting interface
  //////////////////////////////////////

  void SetLightingGroup(const LightingGroup* lgroup) { mpLightingGroup = lgroup; }
  const LightingGroup* GetLightingGroup() const { return mpLightingGroup; }
  void BindLightMap(Texture* ptex) { mLightMap = ptex; }
  Texture* GetLightMap() const { return mLightMap; }
  bool IsLightMapped() const { return (mLightMap != 0); }
  bool IsVertexLit() const { return mbVertexLit; }


private:
  int miMaterialIndex;
  int miMaterialPassIndex;
  const IRenderer* mpActiveRenderer;
  const IRenderableDag* mpDagRenderable;

  const LightingGroup* mpLightingGroup;
  const XgmMaterialStateInst* mMaterialInst;
  Texture* mDPTopEnvMap;
  Texture* mDPBotEnvMap;
  Texture* mLightMap;
  bool mbIsSkinned;
  bool mbForzeNoZWrite;
  bool mbVertexLit;
  float mEngineParamFloats[kMaxEngineParamFloats];
  RenderGroupState mRenderGroupState;
};

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per frame
//  render modes, target, etc....
///////////////////////////////////////////////////////////////////////////////

typedef svar64_t rendervar_t;

struct RenderContextFrameData {

  RenderContextFrameData(GfxTarget* ptarg);

  RenderContextFrameData(const RenderContextFrameData&) =delete;
  RenderContextFrameData& operator=(const RenderContextFrameData&) =delete;

  GfxTarget* GetTarget(void) const { return mpTarget; }
  LightManager* GetLightManager() const { return _lightmgr; }

  void SetLightManager(LightManager* lmgr) { _lightmgr = lmgr; }

  typedef orklut<CrcString, rendervar_t> usermap_t;
  const usermap_t& userProperties() const { return _userProperties; }
  usermap_t& userProperties() { return _userProperties; }

  void setUserProperty(CrcString, rendervar_t data);
  void unSetUserProperty(CrcString);
  rendervar_t getUserProperty(CrcString prop) const;

  const DrawableBuffer* GetDB() const;

  const CompositingPassData& topCPD() const;

  //////////////////////////////////////

  CompositingImpl* _cimpl = nullptr;
  LightManager* _lightmgr = nullptr;
  usermap_t _userProperties;
  GfxTarget* const mpTarget = nullptr;
  const IRenderer* _renderer;
};

typedef std::function<void(RenderContextFrameData&)> PreRenderCallback_t;

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
