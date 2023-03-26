////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <functional>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/math/TransformNode.h>
#include <ork/math/cvector4.h>
#include <ork/math/plane.h>
#include <ork/object/Object.h>
#include <ork/rtti/Class.h>
#include <ork/lev2/gfx/gfxmodel.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
//
// A Renderable is an object that renders itself in an atomic, Render call. When drawing
// a scene, the majority of objects drawn are in the form of a Renderable. Renderables
// are Retained-Mode-style objects that compose the Scene Graph.
//
// Use Render() to draw the object immediately. Use Queue() to place the object in a
// RenderQueue for deferred, sorted rendering based on a sort key.
//
// Example Renderables are skyboxes, ground/water planes, and meshes.
//////////////////////////////////////////////////////////////////////////////

struct IRenderable {

  using var_t                                  = svar32_t;
  static constexpr int kManipRenderableSortKey = 0x7fffffff;
  static constexpr int kLastRenderableSortKey  = 0x7ffffffe;
  static constexpr int kFirstRenderableSortKey = 0;
  //////////////////////////////////////////////////////////////////////////////
  IRenderable();
  virtual ~IRenderable();
  //////////////////////////////////////////////////////////////////////////////
  void SetObject(const ork::Object* o);
  const ork::Object* GetObject() const;
  const fcolor4& GetModColor() const;
  void SetModColor(const fcolor4& color);
  void SetMatrix(const fmtx4& mtx);
  const fmtx4& GetMatrix() const;
  void SetDrawableDataA(const var_t& ap);
  const var_t& GetDrawableDataA() const;
  void SetDrawableDataB(const var_t& ap);
  const var_t& GetDrawableDataB() const;
  //////////////////////////////////////////////////////////////////////////////
  virtual void Render(const IRenderer* renderer) const = 0;
  //////////////////////////////////////////////////////////////////////////////
  /// Renderables implement this function to set the sort key used when all Renderables are sorted together.
  /// The default is 0 for all Renderables. If no Renderable overrides this, then the RenderableQueue is not
  /// sorted and all Renderables are drawn in the order they are queued.
  /// Typically, a Renderable will use the IRenderer::ComposeSortKey() function as a helper when composing
  /// its sort key.
  virtual uint32_t ComposeSortKey(const IRenderer* renderer) const;
  //////////////////////////////////////////////////////////////////////////////

  matrix_lamda_t genMatrixLambda() const;

  const ork::Object* _object = nullptr;
  bool _instanced            = false;

  fmtx4 _worldMatrix;
  fcolor4 _modColor;
  var_t _drawDataA;
  var_t _drawDataB;
};

///////////////////////////////////////////////////////////////////////////////

struct ModelRenderable : public IRenderable {

  static const int kMaxEngineParamFloats = ork::lev2::RenderContextInstData::kMaxEngineParamFloats;

  ModelRenderable(IRenderer* renderer = NULL);

  void SetEngineParamFloat(int idx, float fv);
  float GetEngineParamFloat(int idx) const;
  uint32_t ComposeSortKey(const IRenderer* renderer) const final;
  void Render(const IRenderer* renderer) const final;

  xgmsubmeshinst_ptr_t _submeshinst;

  xgmmodelinst_constptr_t _modelinst;
  uint32_t _sortkey      = 0;
  int mSubMeshIndex      = 0;
  int mMaterialIndex     = 0;
  int mMaterialPassIndex = 0;
  int mEdgeColor         = -1;
  float _scale           = 1.0f;

  float mEngineParamFloats[kMaxEngineParamFloats];

  fvec3 _offset;
  fquat _orientation;
  xgmcluster_ptr_t _cluster;
};

///////////////////////////////////////////////////////////////////////////////

struct CallbackRenderable : public IRenderable {

  using cbtype_t = std::function<void(lev2::RenderContextInstData& RCID)>;

  CallbackRenderable(IRenderer* renderer = NULL);

  void SetSortKey(uint32_t skey);
  void SetRenderCallback(cbtype_t cb);
  cbtype_t GetRenderCallback() const;
  void Render(const IRenderer* renderer) const final;
  uint32_t ComposeSortKey(const IRenderer* renderer) const final;

  uint32_t mSortKey;
  int mMaterialIndex;
  int mMaterialPassIndex;
  cbtype_t mRenderCallback;
  const CallbackDrawable* _drawable;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
