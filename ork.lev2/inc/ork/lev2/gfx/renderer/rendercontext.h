////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxenv.h>
#include "renderer_enum.h"
#include <ork/util/crc.h>

namespace ork {
class CameraData;
}
namespace ork::lev2 {

class IRenderer;
class Camera;
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

class RenderContextInstData {
public:
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

struct StereoCamera {
  const CameraData* _left = nullptr;
  const CameraData* _right = nullptr;
  const CameraData* _mono = nullptr;

  fmtx4 VL() const;
  fmtx4 VR() const;
  fmtx4 PL() const;
  fmtx4 PR() const;
  fmtx4 VPL() const;
  fmtx4 VPR() const;
  fmtx4 VMONO() const;
  fmtx4 PMONO() const;
  fmtx4 VPMONO() const;

  fmtx4 MVPL(const fmtx4& M) const;
  fmtx4 MVPR(const fmtx4& M) const;
  fmtx4 MVPMONO(const fmtx4& M) const;

};

struct RenderContextFrameData {

  RenderContextFrameData();

  GfxTarget* GetTarget(void) const { return mpTarget; }
  const CameraData* cameraData() const { return mCameraData; }
  const CameraData* GetPickCameraData() const { return mPickCameraData; }
  LightManager* GetLightManager() const { return mLightManager; }

  const SRect& GetDstRect() const { return mDstRect; }
  const SRect& GetMrtRect() const { return mMrtRect; }

  void setCameraData(const CameraData* data) { mCameraData = data; }
  void SetPickCameraData(const CameraData* data) { mPickCameraData = data; }
  void SetLightManager(LightManager* lmgr) { mLightManager = lmgr; }
  void SetTarget(GfxTarget* ptarg);
  void SetDstRect(const SRect& rect) { mDstRect = rect; }
  void SetMrtRect(const SRect& rect) { mMrtRect = rect; }
  void setLayerName(const char* layername);
  CameraMatrices& cameraMatrices() { return _cameraMatrices; }
  const CameraMatrices& cameraMatrices() const { return _cameraMatrices; }

  void ClearLayers();
  void AddLayer(const PoolString& layername);
  bool HasLayer(const PoolString& layername) const;

  void PushRenderTarget(IRenderTarget* ptarg);
  IRenderTarget* GetRenderTarget();
  void PopRenderTarget();

  typedef orklut<CrcString, rendervar_t> usermap_t;
  const usermap_t& userProperties() const { return _userProperties; }
  usermap_t& userProperties() { return _userProperties; }

  void setUserProperty(CrcString, rendervar_t data);
  void unSetUserProperty(CrcString);
  rendervar_t getUserProperty(CrcString prop) const;

  const DrawableBuffer* GetDB() const;

  void addStandardLayers();

  //////////////////////////////////////

  bool isPicking() const;
  bool isStereoOnePass() const { return _stereo1pass; }
  void setStereoOnePass(bool ena) { _stereo1pass=ena; }

  //////////////////////////////////////

  StereoCamera _stereoCamera;
  orkstack<IRenderTarget*> mRenderTargetStack;
  usermap_t _userProperties;
  LightManager* mLightManager;
  GfxTarget* mpTarget;
  const CameraData* mCameraData;
  const CameraData* mPickCameraData;
  CameraMatrices _cameraMatrices;
  SRect mDstRect;
  SRect mMrtRect;
  orkset<PoolString> mLayers;
  bool _stereo1pass = false;
  const IRenderer* _renderer;
};

typedef std::function<void(RenderContextFrameData&)> PreRenderCallback_t;

///////////////////////////////////////////////////////////////////////////////

} //namespace ork::lev2 {
