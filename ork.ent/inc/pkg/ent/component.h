////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

///////////////////////////////////////////////////////////////////////////////

#include <ork/object/Object.h>
#include <ork/event/EventListener.h>
#include <ork/math/cmatrix4.h>
#include "componentfamily.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class ComponentInst;
class Entity;
class SceneInst;
class ComponentDataTable;
class SceneComponentInst;
class SceneComposer;

///////////////////////////////////////////////////////////////////////////////

class ComponentDataClass : public object::ObjectClass
{
	RttiDeclareExplicit(ComponentDataClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);
public:
	ComponentDataClass(const rtti::RTTIData &);

	PoolString GetFamily() const { return mFamily; }
	void SetFamily(PoolString family) { mFamily = family; }
private:
	PoolString mFamily;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void RegisterFamily(PoolString family)
{
	ork::ent::ComponentDataClass *clazz = ork::rtti::autocast(T::GetClassStatic());
	OrkAssert(clazz);
	clazz->SetFamily(family);
}

///////////////////////////////////////////////////////////////////////////////

class SceneComponentData : public Object
{
	RttiDeclareAbstract( SceneComponentData, Object );
public:

	virtual SceneComponentInst* CreateComponentInst( ork::ent::SceneInst *pinst ) const = 0;

protected:

	SceneComponentData(  ) {}

private:

};

///////////////////////////////////////////////////////////////////////////////

class SceneComponentInst : public Object
{
	RttiDeclareAbstract( SceneComponentInst, Object );
public:

	void Update(SceneInst *inst);
	void Start(SceneInst *psi); 
	void Link(SceneInst* psi); 
	void UnLink(SceneInst* psi); 
	void Stop(SceneInst *psi); 

protected:

	SceneComponentInst( const SceneComponentData* scd, SceneInst *pinst ) : mComponentData(scd), mpSceneInst(pinst), mbStarted(false) {}

	SceneInst*					mpSceneInst;

private:
	
    bool DoNotify(const ork::event::Event *event) override { return false; }
	
    virtual void DoUpdate(SceneInst *inst) {}
	virtual void DoStart(SceneInst *psi) {}
	virtual bool DoLink(SceneInst *psi) { return true; }
	virtual void DoUnLink(SceneInst *psi) {}
	virtual void DoStop(SceneInst *psi) {}

	const SceneComponentData*	mComponentData;
	bool						mbStarted;
};

///////////////////////////////////////////////////////////////////////////////

class ComponentData : public Object
{
	RttiDeclareExplicit(ComponentData, Object, rtti::AbstractPolicy, ComponentDataClass);
public:
	ComponentData();

	virtual ComponentInst* CreateComponent(Entity* pent) const = 0;

	PoolString GetFamily() const;

	void RegisterWithScene( SceneComposer& sc ) { DoRegisterWithScene(sc); }

	virtual const char* GetShortSelector() const { return 0; }

private:

	virtual void DoRegisterWithScene( SceneComposer& sc ) {}
};

///////////////////////////////////////////////////////////////////////////////

class ComponentInst : public Object
{
	RttiDeclareAbstract( ComponentInst, Object );
public:

	void SetEntity(Entity *entity) { mEntity = entity; }
	Entity *GetEntity() { return mEntity; }
	const Entity *GetEntity() const { return mEntity; }
	//Shortcut to make debugging printfs easier
	const char* GetEntityName() const;

	PoolString GetFamily() const;

	void Update(SceneInst *inst);
	void Start(SceneInst *psi, const CMatrix4 &world); 
	void Link(SceneInst* psi); 
	void UnLink(SceneInst* psi); 
	void Stop(SceneInst *psi); // { DoStop(psi); }

	const char* GetShortSelector() const { return (mComponentData!=0) ? mComponentData->GetShortSelector() : 0; }

protected:

	ComponentInst( const ComponentData* data, Entity *entity );

private:
    
    bool DoNotify(const ork::event::Event *event) override { return false; }
	
    virtual void DoUpdate(SceneInst *inst) {}
	virtual bool DoStart(SceneInst *psi, const CMatrix4 &world) { return true; }
	virtual bool DoLink(SceneInst *psi) { return true; }
	virtual void DoUnLink(SceneInst *psi) {}
	virtual void DoStop(SceneInst *psi) {}

	const ComponentData*	mComponentData;
	Entity*					mEntity;
	bool					mbStarted;
	bool					mbValid;
};

///////////////////////////////////////////////////////////////////////////////

class EditorPropMapInst : public ComponentInst
{
	RttiDeclareAbstract(EditorPropMapInst, ComponentInst);
public:
	EditorPropMapInst( const ComponentData* cd, Entity* pent ) : ComponentInst(cd,pent) {}
};

// TODO: This does not belong here. Put it somewhere else.
class EditorPropMapData : public ComponentData
{
	RttiDeclareConcrete(EditorPropMapData, ComponentData);
public:
	EditorPropMapData();

	void SetProperty(const ConstString &key, const ConstString &val);
	ConstString GetProperty(const ConstString &key) const;

private:

	ComponentInst* CreateComponent(Entity* pent) const final { return new EditorPropMapInst(this,pent); }

	orklut<ConstString, ConstString> mProperties;
};


///////////////////////////////////////////////////////////////////////////////

} }
