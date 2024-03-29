////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include "component.h"
#include "componentfamily.h"
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/any.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/tempstring.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

namespace lev2 {
class CameraData;
class XgmModel;
class XgmModelAsset;
class XgmModelInst;
class IRenderer;
class LightManager;
} // namespace lev2

namespace ent {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ModelComponentData : public ComponentData {
  RttiDeclareConcrete(ModelComponentData, ComponentData);

public:
  typedef orklut<PoolString, PoolString> mtloverridemap_t;

  ModelComponentData();

  lev2::XgmModel* GetModel() const;
  void SetModel(lev2::XgmModelAsset* mdl);

  void SetAlwaysVisible(bool always) {
    mAlwaysVisible = always;
  }
  bool IsAlwaysVisible() const {
    return mAlwaysVisible;
  }
  float GetScale() const {
    return mfScale;
  }
  void SetScale(float v) {
    mfScale = v;
  }
  const fvec3& GetRotate() const {
    return mRotate;
  }
  const fvec3& GetOffset() const {
    return mOffset;
  }
  void SetRotate(const fvec3& r) {
    mRotate = r;
  }

  const mtloverridemap_t& MaterialOverrideMap() const {
    return _materialOverrides;
  }

  bool ShowBoundingSphere() const {
    return mbShowBoundingSphere;
  }

  bool IsCopyDag() const {
    return mbCopyDag;
  }

  bool IsBlenderZup() const {
    return mBlenderZup;
  }

  ComponentInst* createComponent(Entity* pent) const final;

private:
  void GetModelAccessor(ork::rtti::ICastable*& model) const;
  void SetModelAccessor(ork::rtti::ICastable* const& model);

  bool mAlwaysVisible;
  float mfScale;
  fvec3 mOffset;
  fvec3 mRotate;
  lev2::XgmModelAsset* mModel;
  bool mbShowBoundingSphere;
  bool mbCopyDag;
  bool mBlenderZup;

  mtloverridemap_t _materialOverrides;
};

///////////////////////////////////////////////////////////////////////////////

class ModelComponentInst : public ComponentInst {
  RttiDeclareAbstract(ModelComponentInst, ComponentInst);

public:
  ModelComponentInst(const ModelComponentData& data, Entity* pent);
  ~ModelComponentInst();

  void Start();

  lev2::ModelDrawable& modelDrawable() {
    return *mModelDrawable;
  }
  const lev2::ModelDrawable& modelDrawable() const {
    return *mModelDrawable;
  }

  const ModelComponentData& GetData() const {
    return mData;
  }

protected:
  const ModelComponentData& mData;
  lev2::model_drawable_ptr_t mModelDrawable;
  ork::lev2::xgmmodelinst_ptr_t _modelinst;
  bool _yo = false;
  const char* scriptName() final {
    return "ModelComponent";
  }

  void DoUpdate(ork::ent::Simulation* psi) final;
  void doNotify(const ork::event::Event* event) final;
  void DoStop(ork::ent::Simulation* psi) final;
  void doNotify(const ComponentEvent& e) final;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ent
} // namespace ork
