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
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include "componentfamily.h"
#include <pkg/ent/component.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/heightmap.h>
#include <ork/math/PIDController.h>
#include <ork/kernel/orkpool.h>
#include <ork/math/box.h>
#include <ork/kernel/mutex.h>

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

struct PhysicsDebugger : public btIDebugDraw
{
	const orkvector<PhysicsDebuggerLine>& GetLines1() const { return mClearOnBeginInternalTickLines; }
	const orkvector<PhysicsDebuggerLine>& GetLines2() const { return mClearOnRenderLines; }

	PhysicsDebugger();

	void ClearOnBeginInternalTick() { mClearOnBeginInternalTick = true; }
	void ClearOnRender() { mClearOnBeginInternalTick = false; }
	void SetClearOnBeginInternalTick(bool internalTick = true) { mClearOnBeginInternalTick = internalTick; }
	bool IsClearOnBeginInternalTick() { return mClearOnBeginInternalTick; }
	bool IsDebugEnabled() const { return mbDEBUG; }

	void AddLine(const ork::CVector3& from, const ork::CVector3& to, const ork::CVector3& color);
	void Render(ork::lev2::RenderContextInstData &rcid, ork::lev2::GfxTarget *ptarg);
	void Render(ork::lev2::RenderContextInstData &rcid, ork::lev2::GfxTarget *ptarg, const orkvector<PhysicsDebuggerLine> &lines);
	void SetDebug( bool bv ) { mbDEBUG=bv; }
	void BeginInternalTickClear() { mClearOnBeginInternalTickLines.clear(); }
	void RenderClear() { mClearOnRenderLines.clear(); }

	//////////////////////////

	void beginInternalStep() override { BeginInternalTickClear(); }
	void endInternalStep() override {}
	void drawLine(const btVector3& from,const btVector3& to,const btVector3& color) override;
	void drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color) override;
	void reportErrorWarning(const char* warningString) override;
	void draw3dText(const btVector3& location,const char* textString) override;
	void setDebugMode(int debugMode) override;
	int getDebugMode() const override;

	void Lock();
	void UnLock();

private:

	ork::mutex mMutex;
	orkvector<PhysicsDebuggerLine> mClearOnBeginInternalTickLines;
	orkvector<PhysicsDebuggerLine> mClearOnRenderLines;
	bool mClearOnBeginInternalTick;
	bool mbDEBUG;

	//////////////////////////

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
	ComponentInst *CreateComponent(Entity *pent) const override;

};

class BulletWorldControllerInst;

struct BulletDebugDrawDBRec
{
	const ork::ent::Entity*			mpEntity;
	BulletWorldControllerInst*		mpBWCI;
	orkvector<PhysicsDebuggerLine>	mLines1;
	orkvector<PhysicsDebuggerLine>	mLines2;
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


	void DoUpdate(ork::ent::SceneInst* inst) final;
	bool DoNotify(const ork::event::Event *event) final { return false; }

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

	void DoCompose(ork::ent::ArchComposer& composer) override;
	void DoLinkEntity( ork::ent::SceneInst* psi, ork::ent::Entity *pent ) const override;
	void DoStartEntity(ork::ent::SceneInst* psi, const ork::CMatrix4 &world, ork::ent::Entity *pent ) const override;

};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectArchetype : public ork::ent::Archetype
{
	RttiDeclareConcrete(BulletObjectArchetype, ork::ent::Archetype);

private:

    void DoCompose(ork::ent::ArchComposer& composer) override;
	void DoLinkEntity(ork::ent::SceneInst *inst, ork::ent::Entity *pent) const override;
	void DoStartEntity(ork::ent::SceneInst *inst, const ork::CMatrix4 &world, ork::ent::Entity *pent) const override;

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
class BulletShapeBaseData;

class BulletObjectControllerData;

struct ShapeCreateData
{
	Entity* mEntity;
	BulletWorldControllerInst* mWorld;
	BulletObjectControllerInst* mObject;
};

struct ShapeFactory {

    typedef std::function<BulletShapeBaseInst*(const ShapeCreateData& data)> creator_t;
    typedef std::function<void(BulletShapeBaseData*)> invalidator_t;

    ShapeFactory(creator_t c=nullptr,invalidator_t i=[](BulletShapeBaseData*){})
        : _createShape(c)
        , _invalidate(i)
        , _impl(nullptr){

    }

    creator_t _createShape;
    invalidator_t _invalidate;
    svarp_t _impl;
};

typedef ShapeFactory shape_factory_t;

class BulletShapeBaseData : public ork::Object
{
	RttiDeclareAbstract( BulletShapeBaseData, ork::Object );

public:

	BulletShapeBaseData();
	~BulletShapeBaseData();

	BulletShapeBaseInst* CreateShape(const ShapeCreateData& data) const;

protected:

	shape_factory_t mShapeFactory;

	bool DoNotify(const event::Event *event) override;

};

struct BulletShapeBaseInst
{
	BulletShapeBaseInst(const BulletShapeBaseData*data=nullptr)
        : _shapeData(data)
        , mCollisionShape(nullptr)
        , _impl(nullptr) {
    }

	const AABox& GetBoundingBox() const { return mBoundingBox; }
	btCollisionShape* GetBulletShape() const { return mCollisionShape; }

    const BulletShapeBaseData* _shapeData;
	btCollisionShape*		mCollisionShape;
	AABox					mBoundingBox;
    CallbackDrawable*       _drawable = nullptr;
    svar16_t                _impl;
};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeCapsuleData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapeCapsuleData, BulletShapeBaseData );

public:

	BulletShapeCapsuleData();

private:

	float					mfRadius;
	float					mfExtent;

};

///////////////////////////////////////////////////////////////////////////////

class BulletShapePlaneData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapePlaneData, BulletShapeBaseData );

public:

	BulletShapePlaneData();

};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeSphereData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapeSphereData, BulletShapeBaseData );

public:

	BulletShapeSphereData();

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
	float GetScale() const { return mfScale; }

private:

	lev2::XgmModelAsset*	mModelAsset;
	float					mfScale;

};

///////////////////////////////////////////////////////////////////////////////

class BulletShapeHeightfieldData : public BulletShapeBaseData
{
	RttiDeclareConcrete( BulletShapeHeightfieldData, BulletShapeBaseData );

public:

	BulletShapeHeightfieldData();
	~BulletShapeHeightfieldData();

	void SetHeightMapName( file::Path const & lmap );
	void GetHeightMapName( file::Path & lmap ) const;

	const file::Path& HeightMapPath() const { return mHeightMapName; }
	float WorldHeight() const { return mWorldHeight; }
	float WorldSize() const { return mWorldSize; }
	const CVector3& GetVisualOffset() const { return mVisualOffset; }

	ork::lev2::TextureAsset* GetSphereMap() const { return mSphereLightMap; }

private:

	void SetTextureAccessor( ork::rtti::ICastable* const & tex);
	void GetTextureAccessor( ork::rtti::ICastable* & tex) const;

    bool PostDeserialize(reflect::IDeserializer &) final;

	file::Path							mHeightMapName;
	float								mWorldHeight;
	float								mWorldSize;
	CVector3 							mVisualOffset;
	ork::lev2::TextureAsset*			mSphereLightMap;

};

///////////////////////////////////////////////////////////////////////////////

class BulletObjectControllerData : public ComponentData
{
	RttiDeclareConcrete( BulletObjectControllerData, ComponentData );

public:

	BulletObjectControllerData();
	~BulletObjectControllerData();
	float GetRestitution() const { return mfRestitution; }
	float GetFriction() const { return mfFriction; }
	float GetMass() const { return mfMass; }
	float GetAngularDamping() const { return mfAngularDamping; }
	float GetLinearDamping() const { return mfLinearDamping; }
	bool GetAllowSleeping() const { return mbAllowSleeping; }
    bool GetKinematic() const { return mbKinematic; }

	const ork::ObjectMap GetForceControllerData() const { return mForceControllerDataMap; }
	const BulletShapeBaseData* GetShapeData() const;
	void SetShapeData(BulletShapeBaseData*pdata) { mShapeData=pdata; }

	void ShapeGetter(ork::rtti::ICastable*& val) const;
	void ShapeSetter(ork::rtti::ICastable* const & val);

protected:

	friend class BulletObjectControllerInst;

	ComponentInst *CreateComponent(Entity *pent) const override;

	const BulletShapeBaseData*				mShapeData;
	ork::ObjectMap							mForceControllerDataMap;
	float									mfRestitution;
	float									mfFriction;
	float									mfMass;
	float									mfAngularDamping;
	float									mfLinearDamping;
	bool									mbAllowSleeping;
    bool                                    mbKinematic;
		bool _disablePhysics;

};

class BulletObjectControllerInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(BulletObjectControllerInst, ork::ent::ComponentInst);

public:
	BulletObjectControllerInst(const BulletObjectControllerData& data, ork::ent::Entity *entity);
	~BulletObjectControllerInst();

	btRigidBody* GetRigidBody() { return mRigidBody; }
	const BulletObjectControllerData& GetData() const { return mBOCD; }

	const BulletShapeBaseInst* GetShapeInst() const { return mShapeInst; }

private:

	const BulletObjectControllerData&					mBOCD;
	btRigidBody*										mRigidBody;
	orkmap<PoolString,BulletObjectForceControllerInst*>	mForceControllerInstMap;
	BulletShapeBaseInst*								mShapeInst;

	void DoUpdate(ork::ent::SceneInst* inst) final;
	bool DoNotify(const ork::event::Event *event) final { return false; }
	bool DoLink(SceneInst *psi) final;
	void DoStop(SceneInst *psi) final;

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
