////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Grpahics Environment (Driver/HAL)
///////////////////////////////////////////////////////////////////////////////

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

namespace ork {

struct Frustum;

namespace lev2 {

class Light;
class LightMask;
class CameraData;
class XgmCluster;
class XgmSubMesh;
class XgmModel;
class XgmMesh;
class XgmModelInst;
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

  using var_t = svar16_t;

  IRenderable();
  virtual ~IRenderable() {
  }

  virtual void Render(const IRenderer* renderer) const = 0;
  virtual bool CanGroup(const IRenderable* oth) const {
    return false;
  }

  /// Renderables implement this function to set the sort key used when all Renderables are sorted together.
  /// The default is 0 for all Renderables. If no Renderable overrides this, then the RenderableQueue is not
  /// sorted and all Renderables are drawn in the order they are queued.
  /// Typically, a Renderable will use the IRenderer::ComposeSortKey() function as a helper when composing
  /// its sort key.
  virtual uint32_t ComposeSortKey(const IRenderer* renderer) const {
    return 0;
  }

  static constexpr int kManipRenderableSortKey = 0x7fffffff;
  static constexpr int kLastRenderableSortKey  = 0x7ffffffe;
  static constexpr int kFirstRenderableSortKey = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct IRenderableDag : public IRenderable {

  IRenderableDag();

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

  fmtx4 _worldMatrix;
  fcolor4 _modColor;
  var_t _drawDataA;
  var_t _drawDataB;
  const ork::Object* _object;
};

///////////////////////////////////////////////////////////////////////////////

struct ModelRenderable : public IRenderableDag {

  static const int kMaxEngineParamFloats = ork::lev2::RenderContextInstData::kMaxEngineParamFloats;

  ModelRenderable(IRenderer* renderer = NULL);

  inline void SetMaterialIndex(int idx) {
    mMaterialIndex = idx;
  }
  inline void SetMaterialPassIndex(int idx) {
    mMaterialPassIndex = idx;
  }
  inline void SetModelInst(const lev2::XgmModelInst* modelInst) {
    mModelInst = modelInst;
  }
  inline void SetEdgeColor(int edge_color) {
    mEdgeColor = edge_color;
  }
  void SetScale(float scale) {
    mScale = scale;
  }
  inline void SetSubMesh(const lev2::XgmSubMesh* cs) {
    mSubMesh = cs;
  }
  inline void SetCluster(const lev2::XgmCluster* c) {
    mCluster = c;
  }
  inline void SetMesh(const lev2::XgmMesh* m) {
    mMesh = m;
  }

  float GetScale() const {
    return mScale;
  }
  inline const lev2::XgmModelInst* GetModelInst() const {
    return mModelInst;
  }
  inline const fmtx4& GetWorldMatrix() const;
  inline int GetMaterialIndex(void) const {
    return mMaterialIndex;
  }
  inline int GetMaterialPassIndex(void) const {
    return mMaterialPassIndex;
  }
  inline int GetEdgeColor() const {
    return mEdgeColor;
  }
  inline const lev2::XgmSubMesh* subMesh(void) const {
    return mSubMesh;
  }
  inline const lev2::XgmCluster* GetCluster(void) const {
    return mCluster;
  }
  inline const lev2::XgmMesh* mesh(void) const {
    return mMesh;
  }

  void SetSortKey(uint32_t skey) {
    mSortKey = skey;
  }

  void SetRotate(const fvec3& v) {
    mRotate = v;
  }
  void SetOffset(const fvec3& v) {
    mOffset = v;
  }

  const fvec3& GetRotate() const {
    return mRotate;
  }
  const fvec3& GetOffset() const {
    return mOffset;
  }

  void SetEngineParamFloat(int idx, float fv);
  float GetEngineParamFloat(int idx) const;

  uint32_t ComposeSortKey(const IRenderer* renderer) const final {
    return mSortKey;
  }
  void Render(const IRenderer* renderer) const final;
  bool CanGroup(const IRenderable* oth) const final;

  float mEngineParamFloats[kMaxEngineParamFloats];

  const lev2::XgmModelInst* mModelInst;
  uint32_t mSortKey;
  int mSubMeshIndex;
  int mMaterialIndex;
  int mMaterialPassIndex;
  int mEdgeColor;
  float mScale;
  fvec3 mOffset;
  fvec3 mRotate;
  const lev2::XgmSubMesh* mSubMesh;
  const lev2::XgmCluster* mCluster;
  const lev2::XgmMesh* mMesh;
};

///////////////////////////////////////////////////////////////////////////////

class CallbackRenderable : public IRenderableDag {
public:
  using cbtype_t = std::function<void(lev2::RenderContextInstData& rcid, lev2::Context* targ, const CallbackRenderable* pren)>;

  CallbackRenderable(IRenderer* renderer = NULL);

  void SetSortKey(uint32_t skey) {
    mSortKey = skey;
  }

  void SetUserData0(var_t pdata) {
    mUserData0 = pdata;
  }
  const var_t& GetUserData0() const {
    return mUserData0;
  }
  void SetUserData1(var_t pdata) {
    mUserData1 = pdata;
  }
  const var_t& GetUserData1() const {
    return mUserData1;
  }

  void SetRenderCallback(cbtype_t cb) {
    mRenderCallback = cb;
  }
  cbtype_t GetRenderCallback() const {
    return mRenderCallback;
  }

private:
  void Render(const IRenderer* renderer) const final;
  uint32_t ComposeSortKey(const IRenderer* renderer) const final {
    return mSortKey;
  }

  uint32_t mSortKey;
  int mMaterialIndex;
  int mMaterialPassIndex;
  var_t mUserData0;
  var_t mUserData1;
  cbtype_t mRenderCallback;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace lev2
} // namespace ork
