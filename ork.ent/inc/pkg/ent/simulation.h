////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/renderer/drawable.h>

///////////////////////////////////////////////////////////////////////////////
/// Simulation is all the work data associated with running a scene
///////////////////////////////////////////////////////////////////////////////

namespace ork::ent {

class SimulationEvent : public ork::event::Event
{
	RttiDeclareAbstract( SimulationEvent, ork::event::Event );
public:

	enum ESIEvent
	{
		ESIEV_BIND = 0,
		ESIEV_DISABLE_VIEW,
		ESIEV_ENABLE_VIEW,
		ESIEV_DISABLE_UPDATE,
		ESIEV_ENABLE_UPDATE,
		ESIEV_START,
		ESIEV_STOP,
		ESIEV_USER,
	};

	SimulationEvent( Simulation* psi, ESIEvent ev ) : mpSimulation(psi), mEvent(ev) {}
	Simulation* simulation() const { return mpSimulation; }
	ESIEvent GetEvent() const { return mEvent; }
	void SetUserData( const anyp& ud ) { mUserData=ud; }
	const anyp& GetUserData() const { return mUserData; }

private:
	Simulation*	mpSimulation;
	ESIEvent	mEvent;
	anyp		mUserData;

};

///////////////////////////////////////////////////////////////////////////////

struct EntityActivationQueueItem
{
	fmtx4	mMatrix;
	Entity*		mpEntity;

	EntityActivationQueueItem( const fmtx4& mtx = fmtx4::Identity, Entity* pent=0 )
		: mMatrix( mtx )
		, mpEntity( pent )
	{
	}
};

///////////////////////////////////////////////////////////////////////////////

class Simulation final : public ork::Object
{
	RttiDeclareAbstract( Simulation, ork::Object );

public:

	typedef orkmap<PoolString, orklist<ComponentInst*> > ActiveComponentType;
	typedef orklut<systemkey_t,System*> SystemLut;
	typedef orklist<ComponentInst*> ComponentList;
	typedef orkset<Entity*> EntitySet;

	Simulation( const SceneData* sdata, Application *application );
	~Simulation();

	///////////////////////////////////////////////////

	void SetSimulationMode(ESimulationMode emode) { OnSimulationMode(emode); }
	ESimulationMode GetSimulationMode() const { return meSimulationMode; }

	const SceneData &GetData() const { return *mSceneData; }

	Entity* FindEntity(PoolString entity) const;
	Entity* FindEntityLoose(PoolString entity) const;

	///////////////////////////////////////////////////
	template <typename T> T* FindTypedEntityComponent( const PoolString& entname ) const;
	///////////////////////////////////////////////////
	template <typename T> T* FindTypedEntityComponent( const char* entname ) const;
	///////////////////////////////////////////////////

	const EntitySet& GetActiveEntities() const { return mActiveEntities; }
	EntitySet& GetActiveEntities() { return mActiveEntities; }

	const orkmap<PoolString, Entity*>& Entities() const { return mEntities; }

	///////////////////////////////////////////////////

	float GetGameTime() const { return mGameTime; }
	float GetDeltaTime() const { return mDeltaTime; }
	float GetUpDeltaTime() const { return mUpDeltaTime; }

	void SetDeltaTime(float speeduptime)  { mDeltaTime = speeduptime; }

    float random(float mmin, float mmax);

	///////////////////////////////////////////////////

	void setCameraData(const PoolString& name, const lev2::CameraData*camdat);
	const lev2::CameraData* cameraData(const PoolString& name ) const;

	///////////////////////////////////////////////////

	void enqueueDrawablesToBuffer(lev2::DrawableBuffer& buffer) const;

  void updateThreadTick();

	///////////////////////////////////////////////////

	const CompositingSystem* compositingSystem() const;
	CompositingSystem* compositingSystem();

	///////////////////////////////////////////////////

	const ActiveComponentType &GetActiveEntityComponents() const { return mActiveEntityComponents; }
	ActiveComponentType &GetActiveEntityComponents() { return mActiveEntityComponents; }

	const ComponentList& GetActiveComponents(ork::PoolString family) const;
	ComponentList& GetActiveComponents(ork::PoolString family);

	void UpdateActiveComponents(ork::PoolString family );

	void QueueActivateEntity(const EntityActivationQueueItem& item);
	void QueueDeactivateEntity(Entity *entity);

	bool IsEntityActive(Entity* entity) const;

	Application *GetApplication() { return mApplication; }
	const Application *GetApplication() const { return mApplication; }

	void ActivateEntity(ent::Entity* pent);
	void DeActivateEntity(ent::Entity* pent);

  //////////////////////////////////////////////////////////

  void beginRenderFrame() const;
  void endRenderFrame() const;
	float desiredFrameRate() const;
	//////////////////////////////////////////////////////////

	Entity* SpawnDynamicEntity( const ent::EntData* spawn_rec );

	//////////////////////////////////////////////////////////
	// previously virtual interface
	//////////////////////////////////////////////////////////

	ent::Entity* GetEntity( const ent::EntData* ) const;
	void SetEntity( const ent::EntData*, ent::Entity* );
	void Update();

	//////////////////////////////////////////////////////////

	void addSystem( systemkey_t key, System* pcomp );
	void clearSystems();

	template <typename T >
	T* findSystem() const;

	typedef lev2::CameraDataLut CameraDataLut;

	void AddLayer( const PoolString& name, lev2::Layer*player );
	lev2::Layer* GetLayer( const PoolString& name );
	const lev2::Layer* GetLayer( const PoolString& name ) const;

	static const ork::PoolString& EventChannel();

	size_t GetEntityUpdateCount() const { return mEntityUpdateCount; }

private:

	void _compose();
	void _decompose();
	void _link();
	void _unlink();
	void _stage();
	void _unstage();
	void _activate();
	void _deactivate();

	void DecomposeEntities();
	void ComposeEntities();
	void LinkEntities();
	void UnLinkEntities();
	void StartEntities();
	void StopEntities();

	void composeSystems();
	void decomposeSystems();
	void StartSystems();
	void LinkSystems();
	void StopSystems();
	void UnLinkSystems();

	void EnterEditState();
	void EnterPauseState();
	void EnterRunState();

	//////////////////////////////////////////////////////////

protected:

	orkmap<PoolString, Archetype*>			mDynamicArchetypes;

	orkmap<PoolString,lev2::Layer*>				mLayers;
	orkmap<PoolString,Entity*>				mEntities;
	EntitySet								mActiveEntities;
	float									mGameTime;			// current game clock time (stops on pause)
	float									mDeltaTime;			// time since last update (0 on pause)
	float									mPrevDeltaTime;		// time since last update (0 on pause)
	float									mUpTime;			// time since program started (does not stop on pause)
	float									mUpDeltaTime;		// time since last update (even in pause)
	float									mStartTime;			// UpTime when game started
	float									mLastGameTime;
	ComponentList							mEmptyList;
	LockedResource<SystemLut>        		_systems;
	SystemLut _updsyslutcopy;
	mutable SystemLut _rensyslutcopy;
	float									mDeltaTimeAccum;
	float									mfAvgDtAcc;
	float									mfAvgDtCtr;
	size_t 									mEntityUpdateCount;

	CameraDataLut								_cameraDataLUT;		// camera list
	//////////////////////////////////////////////////////////
	ActiveComponentType						mActiveEntityComponents;
	orkvector<EntityActivationQueueItem>	mEntityActivateQueue;
	orkvector<Entity*>						mEntityDeactivateQueue;
	//////////////////////////////////////////////////////////
	void ServiceDeactivateQueue();
	void ServiceActivateQueue();
	//////////////////////////////////////////////////////////
	float ComputeDeltaTime();
	//////////////////////////////////////////////////////////
	void UpdateEntityComponents(const ComponentList& components);

private:
	Application *mApplication;

	ESimulationMode										meSimulationMode;
	const SceneData*									mSceneData;
	void OnSimulationMode(ESimulationMode emode);
	void SlotSceneTopoChanged();

};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ent {
