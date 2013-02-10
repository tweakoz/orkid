////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef	ORK_ENT_BULLET_H
#define ORK_ENT_BULLET_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/object/Object.h>
#include <ork/event/EventListener.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include "componentfamily.h"
#include <pkg/ent/component.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <ork/math/PIDController.h>
#include <ork/kernel/orkpool.h>
#include <ork/math/box.h>

//#define BT_USE_DOUBLE_PRECISION

#include <LinearMath/btIDebugDraw.h>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>

class btDynamicsWorld;
class btCollisionShape;
class btRigidBody;
class btBvhTriangleMeshShape;
class btCompoundShape;
class btBoxShape;
class btSphereShape;
class btDefaultCollisionConfiguration;
class btVector3;

namespace ork { namespace lev2 {
class XgmModel;
class XgmMesh;
class XgmCluster;
class GfxTarget;
class RenderContextInstData;
} }

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

btVector3 orkv3tobtv3( const ork::CVector3& v3 );
ork::CVector3 btv3toorkv3( const btVector3& v3 );
btTransform orkmtx4tobtmtx4( const ork::CMatrix4& mtx ) ;
btMatrix3x3 orkmtx3tobtbasis( const ork::CMatrix3& mtx ) ;
ork::CMatrix4 btmtx4toorkmtx4( const btTransform& mtx ) ;
ork::CMatrix3 btbasistoorkmtx3( const btMatrix3x3& mtx ) ;

inline const btVector3& operator <<= ( btVector3& lhs, const ork::CVector3& v3 ) { lhs = orkv3tobtv3(v3); return lhs; }
inline btVector3 operator ! ( const ork::CVector3& v3 ) { return orkv3tobtv3(v3); }
inline ork::CVector3 operator ! ( const btVector3& v3 ) { return btv3toorkv3(v3); }

inline btTransform operator ! ( const ork::CMatrix4& mtx ) { return orkmtx4tobtmtx4(mtx); }
inline ork::CMatrix4 operator ! ( const btTransform& mtx ) { return btmtx4toorkmtx4(mtx); }

inline btMatrix3x3 operator ! ( const ork::CMatrix3& mtx ) { return orkmtx3tobtbasis(mtx); }
inline ork::CMatrix3 operator ! ( const btMatrix3x3& mtx ) { return btbasistoorkmtx3(mtx); }

struct PhysicsDebuggerLine
{
	CVector3 mFrom;
	CVector3 mTo;
	CVector3 mColor;

	PhysicsDebuggerLine( const CVector3& f, const CVector3& t, const CVector3& c ) : mFrom(f), mTo(t), mColor(c) {}
};

///////////////////////////////////////////////////////////////////////////////

class PhysicsDebugger : public btIDebugDraw
{
	orkvector<PhysicsDebuggerLine> mClearOnBeginInternalTickLines;
	orkvector<PhysicsDebuggerLine> mClearOnRenderLines;
	bool mClearOnBeginInternalTick;

	bool mbDEBUG;

public:

	const orkvector<PhysicsDebuggerLine>& GetLines1() const { return mClearOnBeginInternalTickLines; }
	const orkvector<PhysicsDebuggerLine>& GetLines2() const { return mClearOnRenderLines; }

	PhysicsDebugger() : mClearOnBeginInternalTick(true), mbDEBUG(false) {}

	void ClearOnBeginInternalTick() { mClearOnBeginInternalTick = true; }
	void ClearOnRender() { mClearOnBeginInternalTick = false; }
	void SetClearOnBeginInternalTick(bool internalTick = true) { mClearOnBeginInternalTick = internalTick; }
	bool IsClearOnBeginInternalTick() { return mClearOnBeginInternalTick; }
	bool IsDebugEnabled() const { return mbDEBUG; }

	void AddLine(const ork::CVector3& from, const ork::CVector3& to, const ork::CVector3& color);

	/*virtual*/ void beginInternalStep() { BeginInternalTickClear(); }
	/*virtual*/ void endInternalStep() {}
	/*virtual*/ void drawLine(const btVector3& from,const btVector3& to,const btVector3& color);
	/*virtual*/ void drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color);
	/*virtual*/ void reportErrorWarning(const char* warningString);
	/*virtual*/ void draw3dText(const btVector3& location,const char* textString);
	/*virtual*/ void setDebugMode(int debugMode);
	/*virtual*/ int getDebugMode() const;

	void Render(ork::lev2::RenderContextInstData &rcid, ork::lev2::GfxTarget *ptarg);

	void Render(ork::lev2::RenderContextInstData &rcid, ork::lev2::GfxTarget *ptarg, const orkvector<PhysicsDebuggerLine> &lines);

	void SetDebug( bool bv ) { mbDEBUG=bv; }

	void BeginInternalTickClear() { mClearOnBeginInternalTickLines.clear(); }
	void RenderClear() { mClearOnRenderLines.clear(); }
};

class BulletWorldControllerData : public ComponentData
{
	RttiDeclareConcrete( BulletWorldControllerData, ComponentData );

	float	mfTimeScale;
	float	mSimulationRate;
	bool	mbDEBUG;
	CVector3 mGravity;

public:

	BulletWorldControllerData() : mfTimeScale(1.0f), mbDEBUG(false), mSimulationRate(120.0f) {}

	float GetTimeScale() const { return mfTimeScale; }
	bool IsDebug() const { return mbDEBUG; }
	float GetSimulationRate() const { return mSimulationRate; }
	const CVector3& GetGravity() const { return mGravity; }
	
protected:
	/*virtual*/ ComponentInst *CreateComponent(Entity *pent) const;

};

class BulletWorldControllerInst;

struct BulletDebugDrawDBRec
{
	//ork::CVector3				v[4];
	const ork::ent::Entity*			mpEntity;
	BulletWorldControllerInst*		mpBWCI;
	orkvector<PhysicsDebuggerLine>	mLines1;
	orkvector<PhysicsDebuggerLine>	mLines2;
	//const ShipPhysicsInst*		mpPhysics;
	//ork::CVector3					lloc;
};
struct BulletDebugDrawDBData
{
	ork::ent::Entity*				mpEntity;
	BulletWorldControllerInst*		mpBWCI;

	BulletDebugDrawDBRec			mDBRecs[ork::ent::DrawableBuffer::kmaxbuffers];
	PhysicsDebugger*				mpDebugger;	

	BulletDebugDrawDBData( BulletWorldControllerInst* psi, ork::ent::Entity* pent );
	~BulletDebugDrawDBData();
};

///////////////////////////////////////////////////////////////////////////////

class BulletWorldControllerInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(BulletWorldControllerInst, ork::ent::ComponentInst);

	btDiscreteDynamicsWorld*				mDynamicsWorld;
	btDefaultCollisionConfiguration*		mBtConfig;
	btBroadphaseInterface*					mBroadPhase;
	btCollisionDispatcher*					mDispatcher;
	btSequentialImpulseConstraintSolver*	mSolver;
	const BulletWorldControllerData&		mBWCBD;
	PhysicsDebugger							mDebugger;
	int										mMaxSubSteps;
	int										mNumSubStepsTaken;
	float									mfAvgDtAcc;
	float									mfAvgDtCtr;


	void DoUpdate(ork::ent::SceneInst* inst); // virtual
	bool DoNotify(const ork::event::Event *event) { return false; } // virtual

public:
	BulletWorldControllerInst(const BulletWorldControllerData& data, ork::ent::Entity *entity);
	~BulletWorldControllerInst();

	btRigidBody* AddLocalRigidBody(ork::ent::Entity* pent, btScalar mass
		, const btTransform& startTransform,btCollisionShape* shape);

	btDiscreteDynamicsWorld *GetDynamicsWorld() const { return mDynamicsWorld; }

	void LinkPhysics(ork::ent::SceneInst *inst, ork::ent::Entity *entity);

	void InitWorld();

	PhysicsDebugger& Debugger() { return mDebugger; }
	btDynamicsWorld* BulletWorld() { return mDynamicsWorld; }

	int GetMaxSubSteps() const { return mMaxSubSteps; }
	void SetMaxSubSteps(int maxsubsteps) { mMaxSubSteps = maxsubsteps; }

	int GetNumSubStepsTaken() const { return mNumSubStepsTaken; }

	const BulletWorldControllerData& GetWorldData() const { return mBWCBD; }
};

///////////////////////////////////////////////////////////////////////////////

class EntMotionState : public btMotionState
{
public:

    EntMotionState(const btTransform &initialpos, ork::ent::Entity *entity);

    virtual void getWorldTransform(btTransform &transform) const;

    virtual void setWorldTransform(const btTransform &transform);

protected:

	ork::ent::Entity *mEntity;
    btTransform mTransform;
};

// Class: BulletWorldArchetype
// Use for the "bullet_world" entities.
class BulletWorldArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete( BulletWorldArchetype, ork::ent::Archetype );

	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);
	/*virtual*/ void DoLinkEntity( ork::ent::SceneInst* psi, ork::ent::Entity *pent ) const;
	/*virtual*/ void DoStartEntity(ork::ent::SceneInst* psi, const ork::CMatrix4 &world, ork::ent::Entity *pent ) const;

};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete(BulletObjectArchetype, ork::ent::Archetype);

private:

	/*virtual*/ void DoCompose(ork::ent::ArchComposer& composer);
	/*virtual*/ void DoLinkEntity(ork::ent::SceneInst *inst, ork::ent::Entity *pent) const;
	/*virtual*/ void DoStartEntity(ork::ent::SceneInst *inst, const ork::CMatrix4 &world, ork::ent::Entity *pent) const;

};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectForceControllerInst;
class BulletObjectControllerInst;
class BulletObjectControllerData;

class BulletObjectForceControllerData : public ork::Object
{
	RttiDeclareAbstract( BulletObjectForceControllerData, ork::Object );

public:

	BulletObjectForceControllerData();
	~BulletObjectForceControllerData();

	virtual BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const = 0;

};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectForceControllerInst
{
public:

	BulletObjectForceControllerInst( const BulletObjectForceControllerData& data );
	~BulletObjectForceControllerInst();
	virtual void UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci) = 0;
	virtual bool DoLink(ent::SceneInst *psi) = 0;

private:
	const BulletObjectForceControllerData& mData;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeBaseInst;
class BulletObjectControllerData;

class BulletShapeBaseData : public ork::Object
{
	RttiDeclareAbstract( BulletShapeBaseData, ork::Object );

public:

	BulletShapeBaseData();
	~BulletShapeBaseData();

	virtual btCollisionShape* GetBulletShape() const = 0;

	const AABox& GetBoundingBox() const { return mBoundingBox; }

protected:

	btCollisionShape*		mCollisionShape;
	AABox					mBoundingBox;

	bool DoNotify(const event::Event *event); //virtual
	
};

class BulletShapeBaseInst
{
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeCapsuleData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapeCapsuleData, BulletShapeBaseData );

public:

	BulletShapeCapsuleData() : mfRadius(0.10f), mfExtent(1.0f) {}

	btCollisionShape* GetBulletShape() const; // virtual
	
private:

	float					mfRadius;
	float					mfExtent;

};

///////////////////////////////////////////////////////////////////////////////

class BulletShapePlaneData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapePlaneData, BulletShapeBaseData );

public:

	BulletShapePlaneData(){}

	btCollisionShape* GetBulletShape() const; // virtual
	
private:

};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeSphereData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapeSphereData, BulletShapeBaseData );

public:

	BulletShapeSphereData() : mfRadius(1.0f) {}

	btCollisionShape* GetBulletShape() const; // virtual
	
private:

	float					mfRadius;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeModelData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapeModelData, BulletShapeBaseData );

public:

	BulletShapeModelData();
	~BulletShapeModelData();

	lev2::XgmModelAsset* GetAsset() { return mModelAsset; }
	void SetModelAccessor( ork::rtti::ICastable* const & mdl);
	void GetModelAccessor( ork::rtti::ICastable* & mdl) const;
	btCollisionShape* GetBulletShape() const; // virtual
	float GetScale() const { return mfScale; }
	
private:

	lev2::XgmModelAsset*	mModelAsset;
	float					mfScale;

};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectControllerData : public ComponentData
{
	RttiDeclareConcrete( BulletObjectControllerData, ComponentData );

public:

	//typedef orkmap<PoolString,ork::Object*> ObjectMap;

	BulletObjectControllerData();
	~BulletObjectControllerData();
	float GetRestitution() const { return mfRestitution; }
	float GetFriction() const { return mfFriction; }
	float GetMass() const { return mfMass; }
	float GetAngularDamping() const { return mfAngularDamping; }
	float GetLinearDamping() const { return mfLinearDamping; }
	bool GetAllowSleeping() const { return mbAllowSleeping; }
	
	const ork::ObjectMap GetForceControllerData() const { return mForceControllerDataMap; }
	const BulletShapeBaseData* GetShapeData() const;
	void SetShapeData(BulletShapeBaseData*pdata) { mShapeData=pdata; }
	
	void ShapeGetter(ork::rtti::ICastable*& val) const;
	void ShapeSetter(ork::rtti::ICastable* const & val);
			
protected:
	/*virtual*/ ComponentInst *CreateComponent(Entity *pent) const;
	
	const BulletShapeBaseData*				mShapeData;
	ork::ObjectMap							mForceControllerDataMap;
	float									mfRestitution;
	float									mfFriction;
	float									mfMass;
	float									mfAngularDamping;
	float									mfLinearDamping;
	bool									mbAllowSleeping;

};

class BulletObjectControllerInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(BulletObjectControllerInst, ork::ent::ComponentInst);

public:
	BulletObjectControllerInst(const BulletObjectControllerData& data, ork::ent::Entity *entity);
	~BulletObjectControllerInst();
	
	btRigidBody* GetRigidBody() { return mRigidBody; }
	const BulletObjectControllerData& GetData() const { return mBOCD; }

private:

	const BulletObjectControllerData&					mBOCD;
	btRigidBody*										mRigidBody;
	orkmap<PoolString,BulletObjectForceControllerInst*>	mForceControllerInstMap;

	void DoUpdate(ork::ent::SceneInst* inst); // virtual
	bool DoNotify(const ork::event::Event *event) { return false; } // virtual
	bool DoLink(SceneInst *psi);

};

///////////////////////////////////////////////////////////////////////////////

btBoxShape *XgmModelToBoxShape(const ork::lev2::XgmModel *xgmmodel,float fscale);
btSphereShape *XgmModelToSphereShape(const ork::lev2::XgmModel *xgmmodel,float fscale);
btCompoundShape *XgmModelToCompoundShape(const ork::lev2::XgmModel *xgmmodel,float fscale);
btCompoundShape *XgmMeshToCompoundShape(const ork::lev2::XgmMesh *xgmmesh,float fscale);
btCollisionShape *XgmClusterToBvhTriangleMeshShape(const ork::lev2::XgmCluster &xgmcluster,float fscale);
btCollisionShape* XgmModelToGimpactShape(const ork::lev2::XgmModel *xgmmodel,float fscale);

btVector3 orkv3tobtv3( const ork::CVector3& v3 );
ork::CVector3 btv3toorkv3( const btVector3& v3 );
btTransform orkmtx4tobtmtx4( const ork::CMatrix4& mtx );
ork::CMatrix4 btmtx4toorkmtx4( const btTransform& mtx );
btMatrix3x3 orkmtx3tobtbasis( const ork::CMatrix3& mtx );
ork::CMatrix3 btbasistoorkmtx3( const btMatrix3x3& mtx );

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////

#endif
