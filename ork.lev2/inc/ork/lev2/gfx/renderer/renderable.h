////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

class Light;
class LightMask;
class CameraData;
class Anim;
class IRenderer;
class GfxMaterial;
class Manip;
class RenderContextInstData;
class Context;
class GfxMaterial;
class ManipManager;

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

  using var_t                                  = svar16_t;
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
  virtual bool CanGroup(const IRenderable* oth) const;
  //////////////////////////////////////////////////////////////////////////////
  /// Renderables implement this function to set the sort key used when all Renderables are sorted together.
  /// The default is 0 for all Renderables. If no Renderable overrides this, then the RenderableQueue is not
  /// sorted and all Renderables are drawn in the order they are queued.
  /// Typically, a Renderable will use the IRenderer::ComposeSortKey() function as a helper when composing
  /// its sort key.
  virtual uint32_t ComposeSortKey(const IRenderer* renderer) const;
  //////////////////////////////////////////////////////////////////////////////

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

  void SetMaterialIndex(int idx);
  void SetMaterialPassIndex(int idx);
  void SetModelInst(xgmmodelinst_constptr_t modelInst);
  void SetEdgeColor(int edge_color);
  void SetScale(float scale);
  void SetSubMesh(const XgmSubMesh* cs);
  void SetMesh(const XgmMesh* m);
  float GetScale() const;
  xgmmodelinst_constptr_t GetModelInst() const;
  int GetMaterialIndex(void) const;
  int GetMaterialPassIndex(void) const;
  int GetEdgeColor() const;
  const XgmSubMesh* subMesh(void) const;
  xgmcluster_ptr_t GetCluster(void) const;
  const XgmMesh* mesh(void) const;
  void SetSortKey(uint32_t skey);
  void SetRotate(const fvec3& v);
  void SetOffset(const fvec3& v);
  const fvec3& GetRotate() const;
  const fvec3& GetOffset() const;
  void SetEngineParamFloat(int idx, float fv);
  float GetEngineParamFloat(int idx) const;
  uint32_t ComposeSortKey(const IRenderer* renderer) const final;
  void Render(const IRenderer* renderer) const final;
  bool CanGroup(const IRenderable* oth) const final;

  float mEngineParamFloats[kMaxEngineParamFloats];

  xgmmodelinst_constptr_t _modelinst;
  uint32_t mSortKey;
  int mSubMeshIndex;
  int mMaterialIndex;
  int mMaterialPassIndex;
  int mEdgeColor;
  float mScale;
  fvec3 mOffset;
  fvec3 mRotate;
  const XgmSubMesh* mSubMesh;
  xgmcluster_ptr_t _cluster;
  const XgmMesh* mMesh;
};

///////////////////////////////////////////////////////////////////////////////

struct CallbackRenderable : public IRenderable {

  using cbtype_t = std::function<void(lev2::RenderContextInstData& RCID)>;

  CallbackRenderable(IRenderer* renderer = NULL);

  void SetSortKey(uint32_t skey);
  void SetUserData0(var_t pdata);
  const var_t& GetUserData0() const;
  void SetUserData1(var_t pdata);
  const var_t& GetUserData1() const;
  void SetRenderCallback(cbtype_t cb);
  cbtype_t GetRenderCallback() const;
  void Render(const IRenderer* renderer) const final;
  uint32_t ComposeSortKey(const IRenderer* renderer) const final;

  uint32_t mSortKey;
  int mMaterialIndex;
  int mMaterialPassIndex;
  var_t mUserData0;
  var_t mUserData1;
  cbtype_t mRenderCallback;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
