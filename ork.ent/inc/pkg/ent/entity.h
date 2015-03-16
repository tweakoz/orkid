////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/object/Object.h>

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>

#include <ork/kernel/string/ArrayString.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

class DagComponent;
class Archetype;
class Drawable;
class SceneInst;
class SceneData;
class Layer;

///////////////////////////////////////////////////////////////////////////////

class SceneObjectClass 
	: public object::ObjectClass
{
	RttiDeclareExplicit(SceneObjectClass, object::ObjectClass, rtti::NamePolicy, object::ObjectCategory);

public:
	SceneObjectClass(const rtti::RTTIData &data) : object::ObjectClass(data)
		{ SetPreferredName("SceneObject"); }
	ork::ConstString GetPreferredName() const
		{ return mPreferredName; }
	void SetPreferredName(PieceString ps)
		{ mPreferredName = ps; }
protected:
	ArrayString<64> mPreferredName;
};

///////////////////////////////////////////////////////////////////////////////

class SceneObject : public ork::Object
{
	RttiDeclareExplicit(SceneObject, ork::Object, ork::rtti::AbstractPolicy, ent::SceneObjectClass);

protected:

	SceneObject();

public:

	void SetName(PoolString name );
	PoolString GetName() const { return mName; }
	void SetName( const char * name );

private:

	PoolString		mName;
};

///////////////////////////////////////////////////////////////////////////////

class DagRenderableContextData
{
	U32 mRenderFlags;

public:

	DagRenderableContextData() : mRenderFlags( 0 ) {}
	void SetRenderFlags( U32 uval ) { mRenderFlags=uval; }
	U32  GetRenderFlags( void ) const { return mRenderFlags; }
};

///////////////////////////////////////////////////////////////////////////////

class DagNode : public ork::Object
{
	RttiDeclareConcrete( DagNode, ork::Object );

	const ork::rtti::ICastable*				mpOwner;
	orkvector<DagNode*>						mChildren;

protected:

	TransformNode							mTransformNode3D;
	DagRenderableContextData				mRenderableContextData;

	static const int knumtimedmtx = 3;
	CMatrix4								mPrevMtx[knumtimedmtx];
	float									mTimeStamps[knumtimedmtx];

public:



	DagNode( const ork::rtti::ICastable* powner=0 );
	const TransformNode&					GetTransformNode() const { return mTransformNode3D; }
	TransformNode&							GetTransformNode() { return mTransformNode3D; }
	const DagRenderableContextData&			GetRenderableContextData() const { return mRenderableContextData; }
	DagRenderableContextData&				GetRenderableContextData() { return mRenderableContextData; }
	const ork::rtti::ICastable*				GetOwner() const { return mpOwner; }
	orkvector<DagNode*>&					GetChildren() { return mChildren; }
	void									GetMatrix(	ork::CMatrix4& mtx ) const { mTransformNode3D.GetMatrix(mtx); }
	const CMatrix4& 						GetTimedMatrix(int idx) const
	{	OrkAssert( idx < knumtimedmtx );
		return mPrevMtx[idx];
	}
	void									StepTimedMatrices(float ftime);
	void AddChild( DagNode* pchild );
	void RemoveChild( DagNode* pchild );

	void CopyTransformMatrixFrom( const DagNode& other );
	void SetTransformMatrix( const CMatrix4& mtx );
};


///////////////////////////////////////////////////////////////////////////////

class SceneDagObject : public SceneObject
{
	RttiDeclareConcrete( SceneDagObject, SceneObject );
	DagNode		mDagNode;
	PoolString	mParentName;

public:
	SceneDagObject();
	~SceneDagObject();

	void SetParentName( const PoolString& pname );
	const PoolString& GetParentName() const { return mParentName; }

	DagNode& GetDagNode() { return mDagNode; }
	const DagNode& GetDagNode() const { return mDagNode; }

	ork::Object *AccessDagNode() { return & mDagNode; }

};

///////////////////////////////////////////////////////////////////////////////

class SceneGroup : public SceneDagObject
{
	RttiDeclareConcrete( SceneGroup, SceneDagObject );

public:
	SceneGroup();
	~SceneGroup();

	////////////////////////////////////////////////////////////////

	const orkvector<SceneDagObject*>& Children() const { return mChildren; }

	void AddChild( SceneDagObject* pchild );
	void RemoveChild( SceneDagObject* pchild );

	////////////////////////////////////////////////////////////////

	void UnGroupAll();

private:
	orkvector<SceneDagObject*>				mChildren;
};

///////////////////////////////////////////////////////////////////////////////

class EntData : public SceneDagObject
{
	RttiDeclareConcrete( EntData, SceneDagObject );

public:

	EntData();
	virtual ~EntData();

	virtual bool PostDeserialize(reflect::IDeserializer &);

	const Archetype* GetArchetype() const { return mArchetype; }
	void SetArchetype(const Archetype*parch);

	void ArchetypeGetter(ork::rtti::ICastable*& val) const;// { val=mArchetype; }
	void ArchetypeSetter(ork::rtti::ICastable* const & val); // { mArchetype=rtti::downcast<Archetype*>(val); }

	template <typename T> const T* GetTypedComponent() const;

	ConstString GetUserProperty(const ConstString& key) const;

private:

	void SlotArchetypeDeleted( const ork::ent::Archetype *parch );

	const Archetype*						mArchetype;
	orklut<ConstString, ConstString>		mUserProperties;

};

///////////////////////////////////////////////////////////////////////////////
// an INSTANCE of an EntData is an Entity
///////////////////////////////////////////////////////////////////////////////
class Entity : public ork::Object
{
	RttiDeclareAbstract( Entity, ork::Object );
public:
	typedef orkvector<Drawable *> DrawableVector;
	typedef orklut<PoolString,DrawableVector*> LayerMap;

	////////////////////////////////////////////////////////////////
	// Component Interface

	const ComponentTable& GetComponents() const;
	ComponentTable& GetComponents();
	Entity *Self() { return this; }
	void PrintName();
	CVector3 GetEntityPosition() const; //e this should eb gone BUT some entities have NO componenets

	template <typename T> T* GetTypedComponent( bool bsubclass=false );
	template <typename T> const T* GetTypedComponent( bool bsubclass=false ) const;
	
	ComponentInst *GetComponentByClass(rtti::Class *clazz);
	ComponentInst *GetComponentByClassName(ork::PoolString classname);

	const DagNode& GetDagNode() const { return mDagNode; }
	DagNode& GetDagNode() { return mDagNode; }

	CMatrix4 GetEffectiveMatrix() const; // get Entity matrix if scene is running, EntData matrix if scene is stopped
	void SetDynMatrix( const CMatrix4& mtx ); // set this (Entity) matrix

	void AddDrawable( const PoolString& layername, Drawable* pdrw );// { mDrawable.push_back(pdrw); }

	DrawableVector* GetDrawables( const PoolString& layer );
	const DrawableVector* GetDrawables( const PoolString& layer ) const;

	const LayerMap& GetLayers() const { return mLayerMap; }
	
	const EntData& GetEntData() const { return mEntData; }

	////////////////////////////////////////////////////////////////

	void EntDataGetter(ork::rtti::ICastable*& val) const;

	////////////////////////////////////////////////////////////////

	Entity( const EntData& edata, SceneInst *inst );
	~Entity();

	////////////////////////////////////////////////////////////////

	SceneInst *GetSceneInst() const { return mSceneInst; }

private:

	bool DoNotify(const ork::event::Event *event); // virtual

	SceneInst *mSceneInst;

	const EntData&							mEntData;
	mutable bool							mComposed;
	ComponentTable							mComponentTable;
	ComponentTable::LutType					mComponents;
	//DrawableVector							mDrawable; //e Will this go away?  Could go into a component query at activate
	LayerMap								mLayerMap;
	DagNode									mDagNode;
};

///////////////////////////////////////////////////////////////////////////////

class ReferenceArchetype;
class SceneComposer;

struct ArchComposer
{
	ork::orklut<ork::object::ObjectClass*,ork::Object*>		mComponents;
	ork::ent::Archetype*									mpArchetype;
	SceneComposer&											mSceneComposer;

	template <typename T> T* Register();

	void Register( ork::ent::ComponentData* pdata );
	ArchComposer( ork::ent::Archetype* parch, SceneComposer& scene_composer );
	~ArchComposer();
	
};

///////////////////////////////////////////////////////////////////////////////

class Archetype : public SceneObject
{
	RttiDeclareAbstract( Archetype, SceneObject );

public:

	Archetype();
	~Archetype() { DeleteComponents(); }

	void LinkEntity( SceneInst* psi, Entity *pent ) const;
	void UnLinkEntity( SceneInst* psi, Entity *pent ) const;
	void StartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent ) const;
	void StopEntity(SceneInst* psi, Entity *pent ) const;

	void ComposeEntity( Entity *pent ) const;
	void Compose(SceneComposer& scene_composer);
	void DeCompose();

	void DeleteComponents();

	template <typename T> T* GetTypedComponent();
	template <typename T> const T* GetTypedComponent() const;

	const ComponentDataTable& GetComponentDataTable() const { return mComponentDataTable; }
	ComponentDataTable& GetComponentDataTable() { return mComponentDataTable; }

	void SetSceneData( SceneData* psd ) { mpSceneData=psd; }
	SceneData* GetSceneData() const { return mpSceneData; }

protected:

	virtual void DoComposeEntity(Entity *pent) const;
	virtual void DoDeComposeEntity(Entity *pent) const;
	virtual void DoLinkEntity(SceneInst* psi, Entity *pent) const;
	virtual void DoUnLinkEntity(SceneInst* psi, Entity *pent) const;
	virtual void DoStartEntity(SceneInst* psi, const CMatrix4 &world, Entity *pent) const = 0;
	virtual void DoStopEntity(SceneInst* psi, Entity *pent) const {}

	virtual void DoCompose( ArchComposer& arch_composer ) = 0;

	ComponentDataTable						mComponentDataTable;
	ComponentDataTable::LutType				mComponentDatas;

private:

	bool PostDeserialize(reflect::IDeserializer &); // virtual
	SceneData* mpSceneData;

};

///////////////////////////////////////////////////////////////////////////////

class IEntController
{
public:
	virtual void ComposeEntData( EntData *pentdata ) const = 0;
	virtual void DeComposeEntData( EntData *pentdata ) const = 0;
	virtual Entity* ComposeEntity( const EntData *pentdata ) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

} }
