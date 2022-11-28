////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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

///////////////////////////////////////////////////////////////////////////////

struct RenderingModel {
  RenderingModel(uint32_t id="NONE"_crcu);
  bool isDeferred() const;
  bool isForward() const;
  bool isDeferredPBR() const;
  bool isForwardUnlit() const;
  uint32_t _modelID;
};

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

  fmtx4 worldMatrix() const;

  //////////////////////////////////////
  // material interface
  //////////////////////////////////////

  void SetMaterialInst(const XgmMaterialStateInst* mi);

  int GetMaterialIndex(void) const;               // deprecated
  int GetMaterialPassIndex(void) const;           // deprecated
  void SetMaterialIndex(int idx);                 // deprecated
  void SetMaterialPassIndex(int idx);             // deprecated

  bool _isSkinned                           = false;
  bool _isInstanced = false;
  int miMaterialIndex                       = 0;
  int miMaterialPassIndex                   = 0;
  const IRenderer* mpActiveRenderer         = nullptr;
  const IRenderable* _dagrenderable         = nullptr;
  const RenderContextFrameData* _RCFD       = nullptr;
  const XgmMaterialStateInst* mMaterialInst = nullptr;
  fxinstancecache_constptr_t _fx_instance_cache;

  float mEngineParamFloats[kMaxEngineParamFloats];
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
    return _target;
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

  template <typename T>
  T userPropertyAs(CrcString prop) const{
    return getUserProperty(prop).get<T>();
  }

  const DrawableBuffer* GetDB() const;

  const CompositingPassData& topCPD() const;
  bool hasCPD() const;

  bool isStereo() const;

  //////////////////////////////////////

  compositorimpl_ptr_t _cimpl;
  LightManager* _lightmgr = nullptr;
  usermap_t _userProperties;
  Context* const _target = nullptr;
  const IRenderer* _renderer;
  RenderingModel _renderingmodel;
  pbr::commonstuff_ptr_t _pbrcommon;
};

typedef std::function<void(RenderContextFrameData&)> PreRenderCallback_t;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
