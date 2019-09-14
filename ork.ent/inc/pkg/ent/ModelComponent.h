////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/any.h>
#include "componentfamily.h"
#include "component.h"
#include "drawable.h"
#include <ork/gfx/camera.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class CameraData;

namespace lev2 { class XgmModel; class XgmModelAsset; }
namespace lev2 { class XgmModelInst; }
namespace lev2 { class Renderer; }
namespace lev2 { class LightManager; }
namespace lev2 { class GfxMaterialFx; }
namespace lev2 { class GfxMaterialFxEffectInstance; }

namespace ent {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class ModelComponentData : public ComponentData
{
	RttiDeclareConcrete(ModelComponentData, ComponentData)

public:

	ModelComponentData();

	lev2::XgmModel* GetModel() const;
	void SetModel( lev2::XgmModelAsset* mdl );

	void SetAlwaysVisible(bool always) { mAlwaysVisible = always; }
	bool IsAlwaysVisible() const { return mAlwaysVisible; }
	float GetScale() const { return mfScale; }
	void SetScale( float v ) { mfScale=v; }
	const fvec3& GetRotate() const { return mRotate; }
	const fvec3& GetOffset() const { return mOffset; }
	void SetRotate( const fvec3& r) { mRotate=r; }

	const orklut<PoolString,lev2::FxShaderAsset*>& GetLayerFXMap() const { return mLayerFx; }

	bool ShowBoundingSphere() const { return mbShowBoundingSphere; }

	bool IsCopyDag() const { return mbCopyDag; }

	bool IsBlenderZup() const { return mBlenderZup; }

    ComponentInst *createComponent(Entity *pent) const final;

private:

	void GetModelAccessor(ork::rtti::ICastable *&model) const;
	void SetModelAccessor(ork::rtti::ICastable *const &model);

	bool									mAlwaysVisible;
	float									mfScale;
	fvec3								mOffset;
	fvec3								mRotate;
	lev2::XgmModelAsset*					mModel;
	bool									mbShowBoundingSphere;
	bool									mbCopyDag;
	bool									mBlenderZup;

	orklut<PoolString,lev2::FxShaderAsset*>	mLayerFx;
};

///////////////////////////////////////////////////////////////////////////////

class ModelComponentInst : public ComponentInst
{
	RttiDeclareAbstract(ModelComponentInst, ComponentInst)

public:

	ModelComponentInst(const ModelComponentData &data, Entity *pent);
	~ModelComponentInst();

	void Start();

	ModelDrawable& GetModelDrawable() { return *mModelDrawable; }
	const ModelDrawable& GetModelDrawable() const { return *mModelDrawable; }

	const ModelComponentData &GetData() const { return mData; }


protected:

	const ModelComponentData&								mData;
	ModelDrawable*											mModelDrawable;
	orklut<PoolString,lev2::GfxMaterialFx*>					mFxMaterials;
	ork::lev2::XgmModelInst*								mXgmModelInst;
  bool _yo = false;
  const char* scriptName() final {
      return "ModelComponent";
  }

	void DoUpdate( ork::ent::Simulation* psi ) final;
	bool DoNotify(const ork::event::Event *event) final;
	void DoStop( ork::ent::Simulation* psi ) final;
  void doNotify(const ComponentEvent& e) final;
};

///////////////////////////////////////////////////////////////////////////////

}}
