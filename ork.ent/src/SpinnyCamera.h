////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/math/multicurve.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { class XgmModel; class GfxMaterial3DSolid; } }
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class SequenceCamArchetype : public Archetype
{
	RttiDeclareConcrete( SequenceCamArchetype, Archetype );

	void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	SequenceCamArchetype();

};

///////////////////////////////////////////////////////////////////////////////

class SeqCamItemInstBase;

class SeqCamItemDataBase : public ork::Object
{
	RttiDeclareAbstract( SeqCamItemDataBase, ork::Object );

public:

	SeqCamItemDataBase();
	virtual SeqCamItemInstBase* CreateInst( ork::ent::Entity* pent ) const = 0;

};

///////////////////////////////////////////////////////////////////////////////

class SeqCamItemInstBase 
{

public:
	
	virtual void DoUpdate(ent::SceneInst* sinst) {}

	const SeqCamItemDataBase&	GetCD() const { return mCD; }

	SeqCamItemInstBase( const SeqCamItemDataBase& cd );
	const CCameraData& GetCameraData() const { return mCameraData; }

protected:
	const SeqCamItemDataBase&				mCD;
	CCameraData								mCameraData;

};

///////////////////////////////////////////////////////////////////////////////

class SequenceCamControllerData : public ent::ComponentData
{
	RttiDeclareConcrete( SequenceCamControllerData, ent::ComponentData );

public:

	SequenceCamControllerData();
	const orklut<PoolString,ork::Object*>& GetItemDatas() const { return mItemDatas; }
	PoolString& GetCurrentItem() const { return mCurrentItem; }

private:

    ent::ComponentInst* CreateComponent(ent::Entity* pent) const final;

	orklut<PoolString,ork::Object*>	mItemDatas;
	mutable PoolString				mCurrentItem;

	const char* GetShortSelector() const final { return "sccd"; } 
};

///////////////////////////////////////////////////////////////////////////////

class SequenceCamControllerInst : public ent::ComponentInst
{
	RttiDeclareAbstract( SequenceCamControllerInst, ent::ComponentInst );

public:

	const SequenceCamControllerData&	GetCD() const { return mCD; }
	SequenceCamControllerInst( const SequenceCamControllerData& cd, ork::ent::Entity* pent );

private:
    void DoUpdate(ent::SceneInst* sinst) final;
    bool DoLink(SceneInst *psi) final;
    bool DoStart(SceneInst *psi, const CMatrix4 &world) final;
	bool DoNotify(const ork::event::Event *event) final;
	orklut<PoolString,SeqCamItemInstBase*>	mItemInsts;
	SeqCamItemInstBase*						mpActiveItem;
	const SequenceCamControllerData&		mCD;
	CCameraData								mCameraData;
};

///////////////////////////////////////////////////////////////////////////////

class SpinnyCamControllerData : public SeqCamItemDataBase
{
	RttiDeclareConcrete( SpinnyCamControllerData, SeqCamItemDataBase );

public:

	SpinnyCamControllerData();
	float GetSpinRate() const { return mfSpinRate; }
	float GetElevation() const { return mfElevation; }
	float GetRadius() const { return mfRadius; }
	float GetNear() const { return mfNear; }
	float GetFar() const { return mfFar; }
	float GetAper() const { return mfAper; }
	
private:

    SeqCamItemInstBase* CreateInst( ork::ent::Entity* pent ) const final;

	float									mfSpinRate;
	float									mfElevation;
	float									mfRadius;
	float									mfNear;
	float									mfFar;
	float									mfAper;

};

///////////////////////////////////////////////////////////////////////////////

class SpinnyCamControllerInst : public SeqCamItemInstBase
{
public:
	const SpinnyCamControllerData&	GetSCCD() const { return mSCCD; }

	SpinnyCamControllerInst( const SpinnyCamControllerData& cd, ork::ent::Entity* pent );

private:

	const SpinnyCamControllerData&			mSCCD;
	float									mfPhase;
	
	void DoUpdate(ent::SceneInst* sinst) final;
};

///////////////////////////////////////////////////////////////////////////////

class CurvyCamControllerData : public SeqCamItemDataBase
{
	RttiDeclareConcrete( CurvyCamControllerData, SeqCamItemDataBase );

public:

	CurvyCamControllerData();
	float GetAngle() const { return mfAngle; }
	float GetElevation() const { return mfElevation; }
	float GetRadius() const { return mfRadius; }
	float GetNear() const { return mfNear; }
	float GetFar() const { return mfFar; }
	float GetAper() const { return mfAper; }
	
	const ork::MultiCurve1D& GetRadiusCurve() const { return mRadiusCurve; }

private:

    SeqCamItemInstBase* CreateInst( ork::ent::Entity* pent ) const final;
    ork::Object* RadiusCurveAccessor() { return & mRadiusCurve; }

	float									mfAngle;
	float									mfElevation;
	float									mfRadius;
	float									mfNear;
	float									mfFar;
	float									mfAper;
	ork::MultiCurve1D						mRadiusCurve;

};

///////////////////////////////////////////////////////////////////////////////

class CurvyCamControllerInst : public SeqCamItemInstBase
{
public:
	const CurvyCamControllerData&	GetCCCD() const { return mCCCD; }

	CurvyCamControllerInst( const CurvyCamControllerData& cd, ork::ent::Entity* pent );

private:

	const CurvyCamControllerData&			mCCCD;
	float									mfPhase;
	
	void DoUpdate(ent::SceneInst* sinst) final;
};

///////////////////////////////////////////////////////////////////////////////

} }

