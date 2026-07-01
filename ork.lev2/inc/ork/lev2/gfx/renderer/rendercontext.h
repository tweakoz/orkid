////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
  RenderingModel(uint32_t id = "NONE"_crcu);
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

  static const RenderContextInstData Default;
  static rcid_ptr_t create(rcfd_ptr_t the_rcfd);

  RenderContextInstData(rcfd_ptr_t the_rcfd);

  //////////////////////////////////////
  // renderer interface
  //////////////////////////////////////

  void SetRenderer(const IRenderer* rnd);
  void SetRenderable(const IRenderable* rnd);
  const IRenderer* GetRenderer(void) const;
  void setRenderable(const IRenderable*);
  const XgmMaterialStateInst* GetMaterialInst() const;
  Context* context() const;
  rcfd_ptr_t rcfd() const;
  //////////////////////////////////////

  fmtx4 worldMatrix() const;

  //////////////////////////////////////
  // material interface
  //////////////////////////////////////

  inline void forceTechnique(fxtechnique_constptr_t technique) {
    _forced_technique = technique;
  }

  void SetMaterialInst(const XgmMaterialStateInst* mi);

  int GetMaterialIndex(void) const;     // deprecated
  int GetMaterialPassIndex(void) const; // deprecated
  void SetMaterialIndex(int idx);       // deprecated
  void SetMaterialPassIndex(int idx);   // deprecated

  bool _isSkinned                   = false;
  bool _isInstanced                 = false;
  bool _isSSBOSourced               = false; // vertices pulled from an SSBO (FWD_SSBO_CUSTOM variant)
  bool _isImpostor                  = false; // LOD impostor billboard -> FWD_SSBO_CUSTOM_IMPOSTOR variant
  int miMaterialIndex               = 0;
  int miMaterialPassIndex           = 0;
  const IRenderer* mpActiveRenderer = nullptr;
  const IRenderable* _irenderable   = nullptr;

  fxtechnique_constptr_t _forced_technique = nullptr;
  matrix_lamda_t _genMatrix;
  rcfd_ptr_t _held_rcfd                     = nullptr;
  const XgmMaterialStateInst* mMaterialInst = nullptr;
  fxpipelinecache_constptr_t _pipeline_cache;
  pickvariant_t _pickID;
  pbr::radiancemaps_ptr_t _envmapOverride;  // per-drawable env map override (equirect)
  lightprobe_ptr_t        _probeOverride;   // per-drawable cube probe override
};

///////////////////////////////////////////////////////////////////////////////
// Rendering Context Data that can change per frame
//  render modes, target, etc....
///////////////////////////////////////////////////////////////////////////////

struct RenderContextFrameData {

  RenderContextFrameData(Context* ptarg = nullptr);

  RenderContextFrameData(const RenderContextFrameData&)            = delete;
  RenderContextFrameData& operator=(const RenderContextFrameData&) = delete;

  Context* GetTarget(void) const {
    return context(); // deprecated
  }
  Context* context(void) const {
    return _target;
  }

  typedef orklut<CrcString, rendervar_t> usermap_t;
  const usermap_t& userProperties() const {
    return _userProperties;
  }
  usermap_t& userProperties() {
    return _userProperties;
  }

  bool hasUserProperty(CrcString) const;
  void setUserProperty(CrcString, rendervar_t data);
  void unSetUserProperty(CrcString);

  template <typename T> attempt_cast_const<T> tryUserProperty(CrcString prop) const {
    auto it = _userProperties.find(prop);
    if (it != _userProperties.end())
      return it->second.tryAs<T>();
    return attempt_cast_const<T>(nullptr);
  }

  template <typename T> const T& userPropertyAs(CrcString prop) const {
    auto it = _userProperties.find(prop);
    OrkAssert(it != _userProperties.end());
    return it->second.get<T>();
  }

  const DrawQueue* GetDB() const;

  const CompositingPassData& topCPD() const;
  bool hasCPD() const;

  bool isStereo() const;

  bool renderingModelDebugActive() const;
  void setDebugRenderingModel(uint32_t rmid);
  uint32_t exchangeDebugRenderingModel(uint32_t rmid);
  uint32_t exchangeDebugPassID(uint32_t rmid);
  uint32_t exchangeDebugSubPassID(uint32_t rmid);

  //////////////////////////////////////

  void pushCompositor(compositorimpl_ptr_t c);
  compositorimpl_ptr_t popCompositor();
  compositorimpl_ptr_t topCompositor() const;

  //////////////////////////////////////

  std::stack<compositorimpl_ptr_t> __cimplstack;
  usermap_t _userProperties;
  Context* _target = nullptr;
  const IRenderer* _renderer;
  RenderingModel _renderingmodel;
  RenderingModel _renderingmodelForDebug;
  pbr::commonstuff_ptr_t _pbrcommon;
  std::string _name;
  uint32_t _passID = "NONE"_crcu;
  uint32_t _debugPassID = "NONE"_crcu;
  uint32_t _subpassID = "NONE"_crcu;
  uint32_t _debugSubPassID = "NONE"_crcu;
};

typedef std::function<void(RenderContextFrameData&)> PreRenderCallback_t;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
