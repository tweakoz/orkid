////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_ENT_MODELCOMPONENT_H_
#define _ORK_ENT_MODELCOMPONENT_H_

///////////////////////////////////////////////////////////////////////////////

#include <ork/orkstl.h>
#include <ork/rtti/RTTI.h>
#include <ork/object/Object.h>
#include <ork/object/ObjectClass.h>
#include <ork/lev2/gfx/renderable.h>
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

class CCameraData;

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
	const CVector3& GetRotate() const { return mRotate; }
	const CVector3& GetOffset() const { return mOffset; }
	void SetRotate( const CVector3& r) { mRotate=r; }

	virtual ComponentInst *CreateComponent(Entity *pent) const;

	const orklut<PoolString,lev2::FxShaderAsset*>& GetLayerFXMap() const { return mLayerFx; }

	bool ShowBoundingSphere() const { return mbShowBoundingSphere; }

	bool IsCopyDag() const { return mbCopyDag; }
	
	bool IsBlenderZup() const { return mBlenderZup; }

private:

	void GetModelAccessor(ork::rtti::ICastable *&model) const;
	void SetModelAccessor(ork::rtti::ICastable *const &model);

	bool									mAlwaysVisible;
	float									mfScale;
	CVector3								mOffset;
	CVector3								mRotate;
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

	void DoUpdate( ork::ent::SceneInst* psi ) override; 
	bool DoNotify(const ork::event::Event *event) override;
};

///////////////////////////////////////////////////////////////////////////////

#if 0

class ModelArchetype : public Archetype
{
	RttiDeclareConcrete( ModelArchetype, Archetype );

	/*virtual*/ void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const {}
	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);

public:

	ModelArchetype();

};
#endif
///////////////////////////////////////////////////////////////////////////////

}}

#endif
