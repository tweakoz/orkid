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
#include <ork/kernel/tempstring.h>
#include "componentfamily.h"
#include <ork/event/Event.h>
#include <ork/kernel/any.h>
#include <ork/kernel/future.hpp>
#include <ork/math/cmatrix4.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class CCameraData;
class Application;

namespace lev2 { class XgmModel; }
namespace lev2 { class XgmModelInst; }
namespace lev2 { class Renderer; }
namespace lev2 { class LightManager; }

namespace ent {

///////////////////////////////////////////////////////////////////////////////

enum EUpdateState
{
	EUPD_STOPPED,
	EUPD_START,
	EUPD_RUNNING,
	EUPD_STOP,
};


struct UpdateStatus
{
	UpdateStatus() : meStatus(EUPD_RUNNING) {}
	EUpdateState meStatus;
	void SetState(EUpdateState est);
	EUpdateState GetState() const { return meStatus; }
};

extern UpdateStatus gUpdateStatus;

///////////////////////////////////////////////////////////////////////////////

class Scene;
class EntData;
class Archetype;
class DagNode;
class SceneObject;
class Entity;
class ComponentInst;
class SystemData;
class System;
class Drawable;
class DrawableBuffer;
class Layer;
class CompositingManagerComponentInst;

///////////////////////////////////////////////////////////////////////////////

enum ESceneDataMode
{
	ESCENEDATAMODE_NEW = 0,
	ESCENEDATAMODE_INIT,
	ESCENEDATAMODE_EDIT,
	ESCENEDATAMODE_RUN,
};

enum ESceneInstMode
{
	ESCENEMODE_ATTACHED = 0,	// attached to a SceneData
	ESCENEMODE_EDIT,			// editing
	ESCENEMODE_RUN,				// running
	ESCENEMODE_SINGLESTEP,		// single stepping
	ESCENEMODE_PAUSE,			// pausing
};

///////////////////////////////////////////////////////////////////////////////
/// SceneData is the "model" of the scene that is serialized and edited, and thats it....
/// this should never get subclassed
///////////////////////////////////////////////////////////////////////////////

class SceneData : public ork::Object
{
	RttiDeclareConcrete( SceneData, ork::Object );

public:

	typedef orklut<const ork::object::ObjectClass*,SystemData*> SceneComponentLut;

	SceneData();
	~SceneData(); /*virtual*/

	ESceneDataMode GetSceneDataMode() const { return meSceneDataMode; }

	void AutoLoadAssets() const;

	PoolString NewObjectName() const;

	//////////////////////////////////////////////////////////

	const SceneObject* FindSceneObjectByName(const PoolString& name) const;
	SceneObject* FindSceneObjectByName(const PoolString& name);
	void AddSceneObject(SceneObject* object);
	void RemoveSceneObject(SceneObject* object);
	bool RenameSceneObject(SceneObject* pobj, const char* pname );
	const orkmap<PoolString, SceneObject*> & GetSceneObjects() const { return mSceneObjects; }
	orkmap<PoolString, SceneObject*> & GetSceneObjects() { return mSceneObjects; }

	bool IsSceneObjectPresent(SceneObject*) const;

	////////////////////////////////////////////////////////////////

	template <typename T> T* FindTypedObject( const PoolString& pstr );
	template <typename T> const T* FindTypedObject( const PoolString& pstr ) const;

	template <typename T> std::set<EntData*> FindEntitiesWithComponent() const;
	template <typename T> std::set<EntData*> FindEntitiesOfArchetype() const;

	//////////////////////////////////////////////////////////

	void EnterEditState();
	void EnterInitState();
	void EnterRunState();

	//////////////////////////////////////////////////////////

	template <typename T >
	T* GetTypedSceneComponent() const;

	const SceneComponentLut& GetSceneComponents() const { return mSceneComponents; }
	void AddSceneComponent( SystemData* pcomp );
	void ClearSceneComponents();

	//////////////////////////////////////////////////////////

	file::Path GetScriptPath() const { return mScriptPath; }

private:

	orkmap<PoolString, SceneObject*>		mSceneObjects;
	SceneComponentLut						mSceneComponents;
	ESceneDataMode	meSceneDataMode;
	void OnSceneDataMode(ESceneDataMode emode);
	void PrepareForEdit();
	bool PostDeserialize(reflect::IDeserializer &) final;
	file::Path  							mScriptPath;

};

///////////////////////////////////////////////////////////////////////////////

struct SceneComposer
{
	orklut<const object::ObjectClass*,SystemData*>	mComponents;
	SceneData* mpSceneData;

	template <typename T> T* Register();

	SceneData* GetSceneData() const { return mpSceneData; }

	SceneComposer(SceneData* psd);
	~SceneComposer();

};

///////////////////////////////////////////////////////////////////////////////
/// SceneInst is all the work data associated with running a scene
/// this might be subclassed
///////////////////////////////////////////////////////////////////////////////

class SceneInst;

class SceneInstEvent : public ork::event::Event
{
	RttiDeclareAbstract( SceneInstEvent, ork::event::Event );
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

	SceneInstEvent( SceneInst* psi, ESIEvent ev ) : mpSceneInst(psi), mEvent(ev) {}
	SceneInst* GetSceneInst() const { return mpSceneInst; }
	ESIEvent GetEvent() const { return mEvent; }
	void SetUserData( const anyp& ud ) { mUserData=ud; }
	const anyp& GetUserData() const { return mUserData; }

private:
	SceneInst*	mpSceneInst;
	ESIEvent	mEvent;
	anyp		mUserData;

};

struct EntityActivationQueueItem
{
	CMatrix4	mMatrix;
	Entity*		mpEntity;

	EntityActivationQueueItem( const CMatrix4& mtx = CMatrix4::Identity, Entity* pent=0 )
		: mMatrix( mtx )
		, mpEntity( pent )
	{
	}
};

class SceneInst : public ork::Object
{
	RttiDeclareAbstract( SceneInst, ork::Object );

public:

	typedef orkmap<PoolString, orklist<ComponentInst*> > ActiveComponentType;
	typedef orklut<const ork::object::ObjectClass*,System*> SceneComponentLut;
	typedef orklist<ComponentInst*> ComponentList;
	typedef orkset<Entity*> EntitySet;

	SceneInst( const SceneData* sdata, Application *application );
	~SceneInst();

	///////////////////////////////////////////////////

	void SetSceneInstMode(ESceneInstMode emode) { OnSceneInstMode(emode); }
	ESceneInstMode GetSceneInstMode() const { return meSceneInstMode; }

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

	void SetCameraData(const PoolString& name, const CCameraData*camdat);
	const CCameraData* GetCameraData(const PoolString& name ) const;

	///////////////////////////////////////////////////

	void QueueAllDrawablesToBuffer(ork::ent::DrawableBuffer& buffer) const;
	void RenderDrawableBuffer(lev2::Renderer *renderer,const ork::ent::DrawableBuffer& dbuffer, const PoolString& LayerName ) const;

	///////////////////////////////////////////////////

	CompositingManagerComponentInst* GetCMCI();

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

	Entity* SpawnDynamicEntity( const ent::EntData* spawn_rec );

	//////////////////////////////////////////////////////////
	// previously virtual interface
	//////////////////////////////////////////////////////////

	ent::Entity* GetEntity( const ent::EntData* ) const;
	void SetEntity( const ent::EntData*, ent::Entity* );
	void Update();

	//////////////////////////////////////////////////////////

	void AddSceneComponent( System* pcomp );
	void ClearSceneComponents();

	template <typename T >
	T* FindSystem() const;

	typedef orklut<PoolString,const CCameraData*> CameraLut;

	void AddLayer( const PoolString& name, Layer*player );
	Layer* GetLayer( const PoolString& name );
	const Layer* GetLayer( const PoolString& name ) const;

	static const ork::PoolString& EventChannel();

	size_t GetEntityUpdateCount() const { return mEntityUpdateCount; }

private:

	void DecomposeEntities();
	void ComposeEntities();
	void LinkEntities();
	void UnLinkEntities();
	void StartEntities();
	void StopEntities();

	void ComposeSceneComponents();
	void DecomposeSceneComponents();
	void StartSceneComponents();
	void LinkSceneComponents();
	void StopSceneComponents();
	void UnLinkSceneComponents();

	void EnterEditState();
	void EnterPauseState();
	void EnterRunState();

	//////////////////////////////////////////////////////////

protected:

	orkmap<PoolString, Archetype*>			mDynamicArchetypes;

	orkmap<PoolString,Layer*>				mLayers;
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
	SceneComponentLut						mSceneComponents;
	float									mDeltaTimeAccum;
	float									mfAvgDtAcc;
	float									mfAvgDtCtr;
	size_t 									mEntityUpdateCount;

	CameraLut								mCameraLut;		// camera list

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

	ESceneInstMode										meSceneInstMode;
	const SceneData*									mSceneData;
	void OnSceneInstMode(ESceneInstMode emode);
	void SlotSceneTopoChanged();

};

///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////

} }
