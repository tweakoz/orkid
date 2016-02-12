////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
// The EnemySpawner is the entity which will spawn/despawn enemies,
//  it will also coordinate thier attacks
///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/entity.h>
#include <pkg/ent/rigidbody.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {
class heightfield_rt_inst;
}}

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace wiidom {
///////////////////////////////////////////////////////////////////////////////

class WorldControllerInst;

class EnemySpawnerControllerData : public ent::ComponentData
{
	RttiDeclareAbstract( EnemySpawnerControllerData, ent::ComponentData );

	int									mDebug;
	float								mHeatAdd;
	float								mHeatSpread;
	float								mHeatDecay;
	float								mMaxLinkDist;

	ent::ComponentInst* DoCreateComponent(ent::Entity* pent) const final;

public:


	EnemySpawnerControllerData();
	int GetDebug() const { return mDebug; }
	float GetHeatSpread() const { return mHeatSpread; }
	float GetHeatAdd() const { return mHeatAdd; }
	float GetHeatDecay() const { return mHeatDecay; }
	float GetMaxLinkDistance() const { return mMaxLinkDist; }
};

///////////////////////////////////////////////////////////////////////////////

class FighterControllerInst;
class ShipControllerInst;
struct HotSpotLink;

//static const float kmaxlinkdist = (100.0f);
static const float kminspotdist = (60.0f);
static const float kMaxTemp = 4.0f;

struct HotSpot
{
	float								mCardinalDirWeight[9];
	float								mWeight;
	CVector3							mPosition;
	//orkvector<FighterControllerInst*>	mFighters;
	int									miX, miZ;

	HotSpot();
	
	//void AddFighter(FighterControllerInst*fci);
	//void RemoveFighter(FighterControllerInst*fci);
	CVector3 RequestPosition();

};

///////////////////////////////////////////////////////////////////////////////

struct HotSpotController
{
	static const int khsdim = 32;

	orkvector<HotSpot>					mHotSpots;
	HotSpot*							mClosest;
	HotSpot*							mPrevious;
	float								mHeatAdd;
	float								mHeatSpread;
	float								mHeatDecay;
	int									miSerial;
	int									miSerial2;
	float								mMaxLinkDistance;
	WorldControllerInst*				mWCI;

	void ReadSurface( const CVector3& xyz, CVector3& pos, CVector3& normal ) const;
	
	HotSpotController();

	void UpdateHotSpotLinks();
	HotSpot* GetHotSpot();
	HotSpot* UpdateHotSpot( const CVector3& TargetPos, const CVector3& TargetVel, const CVector3& TargetAcc );

	void Link(WorldControllerInst*wci);

	const orkvector<HotSpot>& HotSpots() const { return mHotSpots; }

	int GetCardinalDir( const CVector2& vxz );
	void SortConnections( const HotSpot* phs, int(&Cardinals)[9] );
	HotSpot* GetConnected( const HotSpot* psrc, int icard );
	HotSpot* GetHotSpot( int ix, int iz );
	void SetWCI( WorldControllerInst*wci ) { mWCI=wci; }

};

///////////////////////////////////////////////////////////////////////////////

class EnemySpawnerControllerInst : public ent::ComponentInst
{
	//DECLARE_TRANSPARENT_ABSTRACT_RTTI( EnemySpawnerControllerInst, ent::ComponentInst );

	const EnemySpawnerControllerData&	mCD;
	ent::Entity*						mTarget;
	WorldControllerInst*				mWCI;
	ShipControllerInst*					mSCI;
	CVector3							mTargetVel;
	CVector3							mTargetAcc;
	int									mNumEnemies;
	float								mfTimer;
	orkvector<FighterControllerInst*>	mFighters;
	virtual void DoUpdate(ent::SceneInst* sinst);
	HotSpot*							mLastHotSpot;
	static const int kmaxhs =			16;
	HotSpotController					mHotSpots;
	bool								mbInit;

public:

	const EnemySpawnerControllerData& GetData() const { return mCD; }

	void Link( ShipControllerInst*sci, WorldControllerInst*wci );

	ShipControllerInst* GetSCI() const { return mSCI; }

	EnemySpawnerControllerInst( const EnemySpawnerControllerData& cd , ork::ent::Entity* pent);

	void SetTarget( ent::Entity*pent ) { mTarget=pent; }

	static void Spawn( ent::SceneInst* sinst, WorldControllerInst*wci, ent::Entity* pte );

	void Despawning( FighterControllerInst* fsi );

	const orkvector<FighterControllerInst*>& Fighters() const { return mFighters; }

	HotSpotController& HotSpots() { return mHotSpots; }
	const HotSpotController& HotSpots() const { return mHotSpots; }

	void ReAssignFighter( FighterControllerInst* fci );


};

///////////////////////////////////////////////////////////////////////////////

class EnemySpawnerArchetype : public ork::ent::Archetype
{
	RttiDeclareAbstract( EnemySpawnerArchetype, ent::Archetype );
		
	void DoCompose(ent::ArchComposer& arch_composer) final;
	void DoLinkEntity( ent::SceneInst* psi, ent::Entity *pent ) const final;
	void DoStartEntity( ent::SceneInst* psi, const ork::CMatrix4& mtx, ent::Entity *pent ) const final {}

public:

	EnemySpawnerArchetype();

};

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
