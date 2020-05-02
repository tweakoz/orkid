////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/crc.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer_enum.h>
#include <ork/lev2/gfx/renderer/compositor.h>

namespace ork::lev2 {

class CameraData;
class CameraMatrices;
class UiCamera;
class IRenderer;
class Texture;
struct LightingGroup;
class LightManager;
class Context;
class OffscreenBuffer;
class RtGroup;
class Window;
class XgmMaterialStateInst;
class IRenderable;
class IRenderTarget;
class DrawableBuffer;
struct RenderContextFrameData;

///////////////////////////////////////////////////////////////////////////////

struct InstancedDrawableData {
  void resize(size_t count);
  std::vector<fmtx4> _worldmatrices;
  std::vector<uint64_t> _pickids;
  std::vector<svar16_t> _miscdata;
};

using instanceddrawdata_ptr_t = std::shared_ptr<InstancedDrawableData>;

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per draw instance
//  ie, per renderable (or even finer grained than that)
///////////////////////////////////////////////////////////////////////////////

struct RenderContextInstData {

  static constexpr int kMaxEngineParamFloats = 4;

  static const RenderContextInstData Default;

  RenderContextInstData(const RenderContextFrameData* RCFD = nullptr);

  //////////////////////////////////////
  // renderer interface
  //////////////////////////////////////

  void SetRenderer(const IRenderer* rnd);
  void SetRenderable(const IRenderable* rnd);
  const IRenderer* GetRenderer(void) const;
  const IRenderable* GetRenderable(void) const;
  const XgmMaterialStateInst* GetMaterialInst() const;
  Context* context() const;

  void SetEngineParamFloat(int idx, float fv);
  float GetEngineParamFloat(int idx) const;

  //////////////////////////////////////
  // material interface
  //////////////////////////////////////

  void SetMaterialInst(const XgmMaterialStateInst* mi);

  int GetMaterialIndex(void) const;               // deprecated
  int GetMaterialPassIndex(void) const;           // deprecated
  void SetMaterialIndex(int idx);                 // deprecated
  void SetMaterialPassIndex(int idx);             // deprecated
  void SetRenderGroupState(RenderGroupState rgs); // deprecated
  RenderGroupState GetRenderGroupState() const;   // deprecated

  int miMaterialIndex;
  int miMaterialPassIndex;
  const IRenderer* mpActiveRenderer;
  const IRenderable* _dagrenderable;

  const XgmMaterialStateInst* mMaterialInst;
  bool _isSkinned;
  float mEngineParamFloats[kMaxEngineParamFloats];
  RenderGroupState mRenderGroupState;
  const RenderContextFrameData* _RCFD = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per frame
//  render modes, target, etc....
///////////////////////////////////////////////////////////////////////////////

typedef svar64_t rendervar_t;

struct RenderContextFrameData {

  RenderContextFrameData(Context* ptarg);

  RenderContextFrameData(const RenderContextFrameData&) = delete;
  RenderContextFrameData& operator=(const RenderContextFrameData&) = delete;

  Context* GetTarget(void) const {
    return context(); // deprecated
  }
  Context* context(void) const {
    return mpTarget;
  }
  LightManager* GetLightManager() const {
    return _lightmgr;
  }

  void SetLightManager(LightManager* lmgr) {
    _lightmgr = lmgr;
  }

  typedef orklut<CrcString, rendervar_t> usermap_t;
  const usermap_t& userProperties() const {
    return _userProperties;
  }
  usermap_t& userProperties() {
    return _userProperties;
  }

  void setUserProperty(CrcString, rendervar_t data);
  void unSetUserProperty(CrcString);
  rendervar_t getUserProperty(CrcString prop) const;

  const DrawableBuffer* GetDB() const;

  const CompositingPassData& topCPD() const;
  bool hasCPD() const;

  bool isStereo() const;

  //////////////////////////////////////

  compositorimpl_ptr_t _cimpl;
  LightManager* _lightmgr = nullptr;
  usermap_t _userProperties;
  Context* const mpTarget = nullptr;
  const IRenderer* _renderer;
};

typedef std::function<void(RenderContextFrameData&)> PreRenderCallback_t;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
