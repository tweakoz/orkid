////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/bullet.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/CollisionShapes/btStaticPlaneShape.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>

#include<ork/math/PIDController.h>

#if defined(_DARWIN)
#include <dispatch/dispatch.h>
#endif

///////////////////////////////////////////////////////////////////////////////

extern bool USE_THREADED_RENDERER;
extern bool bFIXEDTIMESTEP;

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectArchetype, "BulletObjectArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectControllerData, "BulletObjectControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectControllerInst, "BulletObjectControllerInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectForceControllerData, "BulletObjectForceControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeBaseData, "BulletShapeBaseData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeModelData, "BulletShapeModelData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeCapsuleData, "BulletShapeCapsuleData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapePlaneData, "BulletShapePlaneData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeSphereData, "BulletShapeSphereData");


///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void BulletObjectArchetype::Describe()
{
	ork::ArrayString<64> arrstr;
	ork::MutableString mutstr(arrstr);
	mutstr.format("/arch/bullet_object");
	GetClassStatic()->SetPreferredName(arrstr);
}
void BulletObjectArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<BulletObjectControllerData>();
	composer.Register<ork::ent::ModelComponentData>();
}
void BulletObjectArchetype::DoLinkEntity(ork::ent::SceneInst *inst, ork::ent::Entity *pent) const
{
}
void BulletObjectArchetype::DoStartEntity(ork::ent::SceneInst *inst, const ork::CMatrix4 &world, ork::ent::Entity *pent) const
{
}

/////////////////////////////////////////////////////////////////////////////////////

BulletObjectControllerData::BulletObjectControllerData()
	: mfRestitution(0.5f)
	, mfFriction(0.5f)
	, mfMass(1.0f)
	, mfLinearDamping(0.5f)
	, mfAngularDamping(0.5f)
	, mShapeData(0)
	, mbAllowSleeping(true)
{
}
BulletObjectControllerData::~BulletObjectControllerData()
{
//	if( mCollisionShape )
//		delete mCollisionShape;
}

void BulletObjectControllerData::Describe()
{
	ork::ent::RegisterFamily<BulletObjectControllerData>(ork::AddPooledLiteral("bullet"));

	//	reflect::RegisterProperty( "Model", & BulletObjectControllerData::GetModelAccessor, & BulletObjectControllerData::SetModelAccessor );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfRestitution, "Restitution", "0.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfFriction, "Friction", "0.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfMass, "Mass", "0.0", "1000.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfLinearDamping, "LinearDamping", "0.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfAngularDamping, "AngularDamping", "0.0", "1.0" );

	reflect::RegisterProperty("AllowSleeping", &BulletObjectControllerData::mbAllowSleeping);

	reflect::RegisterMapProperty( "ForceControllers", & BulletObjectControllerData::mForceControllerDataMap );
	reflect::AnnotatePropertyForEditor< BulletObjectControllerData >("ForceControllers", "editor.factorylistbase", "BulletObjectForceControllerData" );
	reflect::AnnotatePropertyForEditor< BulletObjectControllerData >("ForceControllers", "editor.map.policy.impexp", "true" );

	reflect::RegisterProperty( "Shape",
								& BulletObjectControllerData::ShapeGetter,
								& BulletObjectControllerData::ShapeSetter );
	reflect::AnnotatePropertyForEditor<BulletObjectControllerData>( "Shape", "editor.factorylistbase", "BulletShapeBaseData" );

}
///////////////////////////////////////////////////////////////////////////////
void BulletObjectControllerData::ShapeGetter(ork::rtti::ICastable*& val) const
{
	BulletShapeBaseData* nonconst = const_cast< BulletShapeBaseData* >( mShapeData );
	val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void BulletObjectControllerData::ShapeSetter( ork::rtti::ICastable* const & val)
{
	ork::rtti::ICastable* ptr = val;
	mShapeData = ( (ptr==0) ? 0 : rtti::safe_downcast<BulletShapeBaseData*>(ptr) );
}
///////////////////////////////////////////////////////////////////////////////
ComponentInst* BulletObjectControllerData::CreateComponent(Entity *pent) const
{
	BulletObjectControllerInst* pinst = new BulletObjectControllerInst( *this, pent );
	return pinst;
}
const BulletShapeBaseData* BulletObjectControllerData::GetShapeData() const
{
	const BulletShapeBaseData* rval = mShapeData;
	return rval;
	
}

/////////////////////////////////////////////////////////////////////////////////////

void BulletObjectControllerInst::Describe()
{
}
BulletObjectControllerInst::BulletObjectControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent)
	: ork::ent::ComponentInst(&data,pent)
	, mBOCD(data)
	, mRigidBody(0)
{
	const orkmap<PoolString,ork::Object*> forcecontrollers = data.GetForceControllerData();

	for( orkmap<PoolString,ork::Object*>::const_iterator	it=forcecontrollers.begin();
															it!=forcecontrollers.end();
															it++ )
	{
		const PoolString& forcename = it->first;
		BulletObjectForceControllerData* forcedata = rtti::autocast(it->second);

		if( forcedata )
		{
			BulletObjectForceControllerInst* force = forcedata->CreateForceControllerInst(data,pent);
			mForceControllerInstMap[forcename]=force;
		}
	}
}				
BulletObjectControllerInst::~BulletObjectControllerInst()
{
}
bool BulletObjectControllerInst::DoLink(SceneInst* psi)
{
	ork::ent::Entity* this_ent = GetEntity();
	
	ork::ent::Entity *bullet_world = psi->FindEntity(ork::AddPooledLiteral("bullet_world"));
	if( bullet_world )
	{
		if(ork::ent::BulletWorldControllerInst *wController
				= bullet_world->GetTypedComponent<ork::ent::BulletWorldControllerInst>(true))
		{
			const BulletWorldControllerData& world_data = wController->GetWorldData();
			btVector3 grav = !world_data.GetGravity();

			if(btDynamicsWorld *world = wController->GetDynamicsWorld())
			{
				const BulletShapeBaseData* shapedata = mBOCD.GetShapeData();
				
				//printf( "SHAPEDATA<%p>\n", shapedata );
				if( shapedata )
				{
					btCollisionShape* pshape = shapedata->GetBulletShape();
					if( pshape )
					{	Entity* ThisEnt = GetEntity();
						////////////////////////////////
						DagNode& dnode = ThisEnt->GetDagNode();
						TransformNode3D& t3d = dnode.GetTransformNode();
						CMatrix4 mtx = t3d.GetTransform()->GetMatrix();
						////////////////////////////////
						btTransform btTrans = ! mtx;
						mRigidBody = wController->AddLocalRigidBody(ThisEnt,mBOCD.GetMass(),btTrans,pshape);
						mRigidBody->setGravity(grav);
						
						bool ballowsleep = mBOCD.GetAllowSleeping();
						mRigidBody->setActivationState(ballowsleep?WANTS_DEACTIVATION:DISABLE_DEACTIVATION);
						mRigidBody->activate();
						//orkprintf( "mRigidBody<%p>\n", mRigidBody );
					}
				}
				world->setGravity(grav);



				for( orkmap<PoolString,BulletObjectForceControllerInst*>::const_iterator	
						it=mForceControllerInstMap.begin();
						it!=mForceControllerInstMap.end();
						it++ )
				{
					BulletObjectForceControllerInst* forcecontroller = it->second;
					//printf( "forcecontroller<%p>\n", forcecontroller );
					if( forcecontroller )
					{
						forcecontroller->DoLink(psi);
					}
				}



			}
		}
	}
	return true;
}

void BulletObjectControllerInst::DoUpdate(ork::ent::SceneInst* inst)
{
	if( mRigidBody )
	{
		//////////////////////////////////////////////////
		// copy motion state to entity transform
		//////////////////////////////////////////////////

		const btMotionState* motionState = mRigidBody->getMotionState();
		btTransform xf;
		motionState->getWorldTransform(xf);
		CMatrix4 xfW = ! xf;
		DagNode& dnode = GetEntity()->GetDagNode();
		TransformNode3D& t3d = dnode.GetTransformNode();
		t3d.GetTransform()->SetMatrix( xfW );

		//////////////////////////////////////////////////
		// update dynamic rigid body params
		//////////////////////////////////////////////////

		mRigidBody->setRestitution(mBOCD.GetRestitution());
		mRigidBody->setFriction(mBOCD.GetFriction());
		mRigidBody->setDamping( mBOCD.GetLinearDamping(), mBOCD.GetAngularDamping() );

		//////////////////////////////////////////////////
		// apply forces
		//////////////////////////////////////////////////

		for( orkmap<PoolString,BulletObjectForceControllerInst*>::const_iterator	
				it=mForceControllerInstMap.begin();
				it!=mForceControllerInstMap.end();
				it++ )
		{
			BulletObjectForceControllerInst* forcecontroller = it->second;
	
			if( forcecontroller )
			{
				forcecontroller->UpdateForces(inst,this);
			}
		}
		//////////////////////////////////////////////////
		//printf( "newpos<%f %f %f>\n", pos.GetX(), pos.GetY(), pos.GetZ() );
	}
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeBaseData::Describe()
{
}
BulletShapeBaseData::BulletShapeBaseData()
	: mCollisionShape(0)
{
}
BulletShapeBaseData::~BulletShapeBaseData()
{
}

bool BulletShapeBaseData::DoNotify(const event::Event *event)
{
	if( const ObjectGedEditEvent* pgev = rtti::autocast(event) )
	{
#if defined(_DARWIN) // TODO fixme (serialize on editorqueue)
		dispatch_async( EditOnlyQueue(), 
		^{
#endif
			if( mCollisionShape ) delete mCollisionShape;
			mCollisionShape = 0;
			printf( "BulletShapeBaseData::Notify() :: DIRTY\n" );
#if defined(_DARWIN)
		});
#endif
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeCapsuleData::Describe()
{
	reflect::RegisterFloatMinMaxProp( &BulletShapeCapsuleData::mfExtent, "Extent", "0.1", "1000.0" );
	reflect::RegisterFloatMinMaxProp( &BulletShapeCapsuleData::mfRadius, "Radius", "0.1", "1000.0" );
}

btCollisionShape* BulletShapeCapsuleData::GetBulletShape() const
{
	BulletShapeCapsuleData* pnonconst = const_cast<BulletShapeCapsuleData*>(this);
	//printf( "NONCONST<%p>\n", pnonconst );
	if( mCollisionShape==0 )
	{
		pnonconst->mCollisionShape = new btCapsuleShapeZ( mfRadius, mfExtent );

		//printf( "mCompoundShape<%p>\n", mCollisionShape );
		btTransform ident;
		btVector3 bbmin;
		btVector3 bbmax;
		mCollisionShape->getAabb( ident, bbmin, bbmax );
		pnonconst->mBoundingBox.SetMinMax( ! bbmin, ! bbmax );
	}
	return mCollisionShape;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapePlaneData::Describe()
{
}
btCollisionShape* BulletShapePlaneData::GetBulletShape() const
{
	BulletShapePlaneData* pnonconst = const_cast<BulletShapePlaneData*>(this);
	//printf( "NONCONST<%p>\n", pnonconst );
	if( mCollisionShape==0 )
	{
		pnonconst->mCollisionShape = new btStaticPlaneShape( ! CVector3::Green(), 0.0f );

		//printf( "mCompoundShape<%p>\n", mCollisionShape );
		btTransform ident;
		btVector3 bbmin;
		btVector3 bbmax;
		mCollisionShape->getAabb( ident, bbmin, bbmax );
		pnonconst->mBoundingBox.SetMinMax( ! bbmin, ! bbmax );
	}
	return mCollisionShape;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeSphereData::Describe()
{
	ork::reflect::RegisterFloatMinMaxProp( &BulletShapeSphereData::mfRadius, "Radius", "0.1", "1000.0" );
}

btCollisionShape* BulletShapeSphereData::GetBulletShape() const
{
	BulletShapeSphereData* pnonconst = const_cast<BulletShapeSphereData*>(this);
	//printf( "NONCONST<%p>\n", pnonconst );
	if( mCollisionShape==0 )
	{
		pnonconst->mCollisionShape = new btSphereShape( mfRadius );

		//printf( "mCompoundShape<%p>\n", mCollisionShape );
		btTransform ident;
		btVector3 bbmin;
		btVector3 bbmax;
		mCollisionShape->getAabb( ident, bbmin, bbmax );
		pnonconst->mBoundingBox.SetMinMax( ! bbmin, ! bbmax );
	}
	return mCollisionShape;
}


///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeModelData::Describe()
{
	reflect::RegisterFloatMinMaxProp( &BulletShapeModelData::mfScale, "Scale", "-1000.0", "1000.0" );

	reflect::RegisterProperty( "Model", & BulletShapeModelData::GetModelAccessor, & BulletShapeModelData::SetModelAccessor );
	reflect::AnnotatePropertyForEditor<BulletShapeModelData>("Model", "editor.class", "ged.factory.assetlist");
	reflect::AnnotatePropertyForEditor<BulletShapeModelData>("Model", "editor.assettype", "xgmodel");
	reflect::AnnotatePropertyForEditor<BulletShapeModelData>("Model", "editor.assetclass", "xgmodel");

}
BulletShapeModelData::BulletShapeModelData()
	: mModelAsset(0)
	, mfScale( 1.0f )
{
}
BulletShapeModelData::~BulletShapeModelData()
{
	if( mCollisionShape ) delete mCollisionShape;
}
btCollisionShape* BulletShapeModelData::GetBulletShape() const
{
	BulletShapeModelData* pnonconst = const_cast<BulletShapeModelData*>(this);
	//printf( "NONCONST<%p>\n", pnonconst );
	if( mCollisionShape==0 )
	{
		if( mModelAsset )
		{
			const lev2::XgmModel* model = mModelAsset->GetModel();
			if( model && model->GetNumMeshes() )
			{
				pnonconst->mCollisionShape = XgmModelToGimpactShape(model,mfScale);
				//pnonconst->mCollisionShape = XgmModelToSphereShape(model,mfScale);

				//printf( "mCompoundShape<%p>\n", mCollisionShape );
				btTransform ident;
				btVector3 bbmin;
				btVector3 bbmax;
				mCollisionShape->getAabb( ident, bbmin, bbmax );
				pnonconst->mBoundingBox.SetMinMax( ! bbmin, ! bbmax );
			}
		}
		else
		{
			if( mCollisionShape ) delete mCollisionShape;
			pnonconst->mCollisionShape = new btSphereShape(mfScale);

		}
	}
	return mCollisionShape;
}

///////////////////////////////////////////////////////////////////////////////
void BulletShapeModelData::SetModelAccessor( ork::rtti::ICastable* const & mdl)
{	mModelAsset = mdl ? ork::rtti::autocast( mdl ) : 0;
	if( mCollisionShape ) delete mCollisionShape;
	mCollisionShape = 0;
}
void BulletShapeModelData::GetModelAccessor( ork::rtti::ICastable* & mdl) const
{	mdl = mModelAsset;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectForceControllerData::Describe()
{
}

BulletObjectForceControllerData::BulletObjectForceControllerData()
{
}
BulletObjectForceControllerData::~BulletObjectForceControllerData()
{
}

BulletObjectForceControllerInst::BulletObjectForceControllerInst( const BulletObjectForceControllerData& data )
	: mData(data)
{

}
BulletObjectForceControllerInst::~BulletObjectForceControllerInst()
{
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


class TestForceControllerData : public BulletObjectForceControllerData
{
	RttiDeclareConcrete( TestForceControllerData, BulletObjectForceControllerData );

public:

	TestForceControllerData()
		: mForce(1.0f)
		, mTorque(0.1f)
		, mOrigin(0.0f,0.0f,0.0f)
		, mPFACTOR(0.0f)
		, mIFACTOR(0.0f)
		, mDFACTOR(0.0f)
		, mfErrPower(1.0f)
	{
	}
	~TestForceControllerData() {}

	BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const; // virtual

	float GetForce() const { return mForce; }
	float GetTorque() const { return mTorque; }
	const CVector3& GetOrigin() const { return mOrigin; }
	float GetPFACTOR() const { return mPFACTOR; }
	float GetIFACTOR() const { return mIFACTOR; }
	float GetDFACTOR() const { return mDFACTOR; }
	PoolString GetTarget() const { return mTarget; }
	float GetErrorPower() const { return mfErrPower; } 

private:

	float					mForce;
	float					mTorque;
	CVector3				mOrigin;
	float					mPFACTOR;
	float					mIFACTOR;
	float					mDFACTOR;
	PoolString				mTarget;
	float					mfErrPower;
};

class MyPid
{
public:
    MyPid()
		: mProportionalFactor(0.075f)
		, mIntegralFactor(0.0f)
		, mDerivativeFactor(0.05f)
		, mIntegralMin(-10.0f)
		, mIntegralMax(+10.0f)
		, mDeltaMin(-1000.0f)
		, mDeltaMax(+1000.0f)
		, mLastError(0.0f)
		, mIntegral(0.0f)
		, mIntegralDecay(0.9f)
	{
	}

    float Update( float MeasuredError, float dt )
	{
		mIntegral += MeasuredError*dt;
		mIntegral *= std::pow(mIntegralDecay, dt);
		mIntegral = maximum(mIntegral, mIntegralMin);
		mIntegral = minimum(mIntegral, mIntegralMax);
		float P =  MeasuredError;
		float D = (MeasuredError - mLastError)/dt;
		float output = P*mProportionalFactor + mIntegral*mIntegralFactor + D*mDerivativeFactor;
		//////////////////////
		// clamp output
		output = (output<mDeltaMin) ? mDeltaMin : output;
		output = (output>mDeltaMax) ? mDeltaMax : output;
		//////////////////////
		mLastError = MeasuredError;
		return output;
	}
	void Configure( float P, float I, float D )
	{
		mProportionalFactor = P;
		mIntegralFactor = I;
		mDerivativeFactor = D;
	}
private:
    float mLastError;
    float mIntegral;

    // these shouldn't change, but not marked const because we might serialize this class.
    float mProportionalFactor;
    float mIntegralFactor;
    float mDerivativeFactor;
	float mIntegralMin;
	float mIntegralMax;
	float mDeltaMin;
	float mDeltaMax;
    float mIntegralDecay;
};

///////////////////////////////////////////////////////////////////////////////

class TestForceControllerInst : public BulletObjectForceControllerInst
{
public:

	TestForceControllerInst( const TestForceControllerData& data );
	~TestForceControllerInst();
	void UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci);
	bool DoLink(SceneInst *psi);

private:
	MyPid							mPIDsteering;
	MyPid							mPIDroll;
	Entity*							mpTarget;
	const TestForceControllerData&	mTestData;
};

///////////////////////////////////////////////////////////////////////////////

void TestForceControllerData::Describe()
{
	reflect::RegisterFloatMinMaxProp( &TestForceControllerData::mForce, "Force", "-8.0", "8.0" );
	reflect::RegisterFloatMinMaxProp( &TestForceControllerData::mTorque, "Torque", "0.0", "100.0" );
	reflect::RegisterFloatMinMaxProp( &TestForceControllerData::mPFACTOR, "PFactor", "-1.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &TestForceControllerData::mIFACTOR, "IFactor", "-1.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &TestForceControllerData::mDFACTOR, "DFactor", "-1.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &TestForceControllerData::mfErrPower, "ErrPower", "0.01", "100.0" );

	reflect::RegisterProperty( "Target", & TestForceControllerData::mTarget );
	reflect::RegisterProperty( "Origin", & TestForceControllerData::mOrigin );
}

BulletObjectForceControllerInst* TestForceControllerData::CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const
{
	TestForceControllerInst* rval = new TestForceControllerInst( *this );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

TestForceControllerInst::TestForceControllerInst( const TestForceControllerData& data )
	: BulletObjectForceControllerInst(data)
	, mTestData(data)
	, mpTarget(0)
{
}
TestForceControllerInst::~TestForceControllerInst()
{
}

bool TestForceControllerInst::DoLink(SceneInst *psi)
{
	mpTarget = psi->FindEntity(mTestData.GetTarget());
	return true;
}

void TestForceControllerInst::UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci)
{
	float fDT = inst->GetDeltaTime();
	const BulletObjectControllerData& BOCD = boci->GetData();
	btRigidBody* rbody = boci->GetRigidBody();
	const btMotionState* motionState = rbody->getMotionState();
	
	float FORCE = mTestData.GetForce();
	CVector3 ORIGIN = mTestData.GetOrigin();
	mPIDsteering.Configure( mTestData.GetPFACTOR(), mTestData.GetIFACTOR(), mTestData.GetDFACTOR() );
	mPIDroll.Configure( mTestData.GetPFACTOR(), mTestData.GetIFACTOR(), mTestData.GetDFACTOR() );

	/////////////////////////////

	if( mpTarget )
	{
		DagNode& dnode = mpTarget->GetDagNode();
		TransformNode3D& t3d = dnode.GetTransformNode();
		CMatrix4 mtx = t3d.GetTransform()->GetMatrix();
		ORIGIN = mtx.GetTranslation();
	}

	/////////////////////////////
	btTransform xf;
	motionState->getWorldTransform(xf);
	CMatrix4 xfW = ! xf;
	CVector3 pos = ! xf.getOrigin();
	CVector3 znormal = xfW.GetZNormal();
	CVector3 xnormal = xfW.GetXNormal();
	CVector3 ynormal = xfW.GetYNormal();
	/////////////////////////////
	const AABox& bbox = BOCD.GetShapeData()->GetBoundingBox();
	CVector3 ctr = (bbox.Min()+bbox.Max())*0.5f;
	CVector3 ctr_bak( ctr.GetX(), ctr.GetY(), bbox.Max().GetZ() );
	CVector3 force_pos = pos;// - ctr_bak;
	CVector3 force_dir = znormal;
	CVector3 force_amt = force_dir*FORCE;
	//rbody->applyForce( ! force_amt, ! force_pos );
	rbody->applyCentralForce( ! force_amt );
	/////////////////////////////
	// Get Direction to Target
	CVector3 dir_to_origin = (ORIGIN-pos).Normal();

	/////////////////////////////
	// Get Torque Axis

	CVector3 Z_torque_vec = znormal.Cross(dir_to_origin);

	///////////////////////////////
	// torque to for ROLL
	{
		CVector3 Y_reference = Z_torque_vec.Cross(dir_to_origin);
		/////////////////////////////
		// the Z_torque_ref_plane is the plane
		// 
		CPlane Z_torque_ref_plane;
		Z_torque_ref_plane.CalcFromNormalAndOrigin( Z_torque_vec, pos ); 
		float ztrpD = Z_torque_ref_plane.GetPointDistance(ORIGIN);
		/////////////////////////////
		// Absolute Error
		float Y_fDOT = ynormal.Dot(Z_torque_vec); // 1 when we are heading to it, -1 when heading away
		float Y_ferrABS = fabs((1.0f-Y_fDOT)*0.5f);
		
		if( Y_ferrABS>0.001f )
		{
			Y_ferrABS = powf( Y_ferrABS, mTestData.GetErrorPower() );
		}
		/////////////////////////////
		// Splitting plane for signed error
		CVector3 Y_split_vec = ynormal.Cross(Z_torque_vec);
		CPlane Y_plane;
		Y_plane.CalcFromNormalAndOrigin( Z_torque_vec, pos ); //! calc given normal and position of plane origin
		/////////////////////////////
		// Signed Error
		float Y_fdistfromsplitplane = Z_torque_ref_plane.GetPointDistance(pos+ynormal);
		float Y_fsign = (Y_fdistfromsplitplane<0.0f) ? 1.0f : -1.0f;
		float Y_ferr = Y_ferrABS*Y_fsign;
		/////////////////////////////
		float Y_famt = mPIDroll.Update(Y_ferr,1.0f);
		float kfmaxT = mTestData.GetTorque()*0.1f;
		if( Y_famt > kfmaxT ) Y_famt = kfmaxT;
		if( Y_famt < -kfmaxT ) Y_famt = -kfmaxT;
		rbody->applyTorque( ! (znormal*Y_famt) );
		/////////////////////////////
		
	}

	///////////////////////////////
	// Z torque
	{
		/////////////////////////////
		// Get Torque Axis
		/////////////////////////////
		// Absolute Error
		float Z_fDOT = znormal.Dot(dir_to_origin); // 1 when we are heading to it, -1 when heading away
		float Z_ferrABS = fabs((1.0f-Z_fDOT)*0.5f);
		
		if( Z_ferrABS>0.001f )
		{
			Z_ferrABS = powf( Z_ferrABS, mTestData.GetErrorPower() );
		}
		/////////////////////////////
		// Splitting plane for signed error
		CVector3 Z_split_vec = znormal.Cross(Z_torque_vec);
		CPlane Z_plane;
		Z_plane.CalcFromNormalAndOrigin( Z_split_vec, pos ); //! calc given normal and position of plane origin
		/////////////////////////////
		// Signed Error
		float Z_fdistfromsplitplane = Z_plane.GetPointDistance(ORIGIN);
		float Z_fsign = (Z_fdistfromsplitplane<0.0f) ? 1.0f : -1.0f;
		float Z_ferr = Z_ferrABS*Z_fsign;
		/////////////////////////////
		float Z_famt = mPIDsteering.Update(Z_ferr,1.0f);
		if( Z_famt > mTestData.GetTorque() ) Z_famt = mTestData.GetTorque();
		if( Z_famt < -mTestData.GetTorque() ) Z_famt = -mTestData.GetTorque();
		rbody->applyTorque( ! (Z_torque_vec*Z_famt) );
		//printf( "rbody<%p> ZTORQUE<%f>\n", rbody, Z_famt );
		/////////////////////////////
	}
//		float famt = (2.0f-(1.0f+fdot));

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class DirectionalForceData : public BulletObjectForceControllerData
{
	RttiDeclareConcrete( DirectionalForceData, BulletObjectForceControllerData );

public:

	DirectionalForceData()
		: mForce(1.0f)
		, mDirection(0.0f,0.0f,0.0f)
	{
	}
	~DirectionalForceData() {}

	BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const; // virtual

	float GetForce() const { return mForce; }
	const CVector3& GetDirection() const { return mDirection; }

private:

	float					mForce;
	CVector3				mDirection;
};

///////////////////////////////////////////////////////////////////////////////

class DirectionalForceInst : public BulletObjectForceControllerInst
{
public:

	DirectionalForceInst( const DirectionalForceData& data );
	~DirectionalForceInst();
	void UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci);
	bool DoLink(SceneInst *psi);

private:
	const DirectionalForceData&	mData;
};

void DirectionalForceData::Describe()
{
	reflect::RegisterProperty( "Force", & DirectionalForceData::mForce );
	reflect::RegisterProperty( "Direction", & DirectionalForceData::mDirection );
	reflect::AnnotatePropertyForEditor< DirectionalForceData >("Force", "editor.range.min", "-1000.0" );
	reflect::AnnotatePropertyForEditor< DirectionalForceData >("Force", "editor.range.max", "1000.0" );
}

BulletObjectForceControllerInst* DirectionalForceData::CreateForceControllerInst(const BulletObjectControllerData& data, ork::ent::Entity* pent) const
{
	DirectionalForceInst* rval = new DirectionalForceInst( *this );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

DirectionalForceInst::DirectionalForceInst( const DirectionalForceData& data )
	: BulletObjectForceControllerInst(data)
	, mData(data)
{
}
DirectionalForceInst::~DirectionalForceInst()
{
}

bool DirectionalForceInst::DoLink(SceneInst *psi)
{
	//mpTarget = psi->FindEntity(mTestData.GetTarget());
	return true;
}

void DirectionalForceInst::UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci)
{
	float fDT = inst->GetDeltaTime();
	const BulletObjectControllerData& BOCD = boci->GetData();
	btRigidBody* rbody = boci->GetRigidBody();
	const btMotionState* motionState = rbody->getMotionState();
	
	float FORCE = mData.GetForce();
	CVector3 DIRECTION = mData.GetDirection();

	/////////////////////////////
	btTransform xf;
	motionState->getWorldTransform(xf);
	CMatrix4 xfW = ! xf;
	CVector3 pos = ! xf.getOrigin();
	CVector3 znormal = xfW.GetZNormal();
	CVector3 xnormal = xfW.GetXNormal();
	CVector3 ynormal = xfW.GetYNormal();
	/////////////////////////////
	const AABox& bbox = BOCD.GetShapeData()->GetBoundingBox();
	CVector3 ctr = (bbox.Min()+bbox.Max())*0.5f;
	CVector3 ctr_bak( ctr.GetX(), ctr.GetY(), bbox.Max().GetZ() );
	CVector3 force_pos = pos;// - ctr_bak;
	CVector3 force_amt = DIRECTION*FORCE;
	//rbody->applyForce( ! force_amt, ! force_pos );
	rbody->applyCentralForce( ! force_amt );
	/////////////////////////////

}

} } // namespace ork { namespace ent

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::TestForceControllerData, "TestForceControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::DirectionalForceData, "DirectionalForceData");
