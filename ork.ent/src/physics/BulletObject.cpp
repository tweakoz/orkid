////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/bullet.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
//#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
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

#include <ork/kernel/opq.h>
#include <pkg/ent/scene.hpp>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectArchetype, "BulletObjectArchetype");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectControllerData, "BulletObjectControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectControllerInst, "BulletObjectControllerInst");
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
void BulletObjectArchetype::DoLinkEntity(ork::ent::Simulation *inst, ork::ent::Entity *pent) const
{
}
void BulletObjectArchetype::DoStartEntity(ork::ent::Simulation *inst, const ork::fmtx4 &world, ork::ent::Entity *pent) const
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
  , mbKinematic(false)
	, _disablePhysics(false)
{
}
BulletObjectControllerData::~BulletObjectControllerData()
{
//	if( mCollisionShape )
//		delete mCollisionShape;
}

void BulletObjectControllerData::DoRegisterWithScene( ork::ent::SceneComposer& sc )
{
	sc.Register<BulletSystemData>();
}

void BulletObjectControllerData::Describe()
{
	//	reflect::RegisterProperty( "Model", & BulletObjectControllerData::GetModelAccessor, & BulletObjectControllerData::SetModelAccessor );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfRestitution, "Restitution", "0.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfFriction, "Friction", "0.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfMass, "Mass", "0.0", "1000.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfLinearDamping, "LinearDamping", "0.0", "1.0" );
	reflect::RegisterFloatMinMaxProp( &BulletObjectControllerData::mfAngularDamping, "AngularDamping", "0.0", "1.0" );

	reflect::RegisterProperty("AllowSleeping", &BulletObjectControllerData::mbAllowSleeping);
    reflect::RegisterProperty("IsKinematic", &BulletObjectControllerData::mbKinematic);
	reflect::RegisterProperty("Disable", &BulletObjectControllerData::_disablePhysics);

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
ComponentInst* BulletObjectControllerData::createComponent(Entity *pent) const
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
	, mShapeInst(nullptr)
{
	if( mBOCD._disablePhysics )
		return;

	pent->simulation()->findSystem<BulletSystem>();

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
	if( mShapeInst )
		delete mShapeInst;
}

BulletObjectForceControllerInst* BulletObjectControllerInst::getForceController(PoolString ps) const {

	BulletObjectForceControllerInst* rval = nullptr;
	auto it = mForceControllerInstMap.find(ps);
	if( it != mForceControllerInstMap.end() ){
		return it->second;
	}
	return nullptr;
}

void BulletObjectControllerInst::DoStop(Simulation* psi)
{
	if( mBOCD._disablePhysics )
		return;

	if( auto bulletsys = psi->findSystem<BulletSystem>() ){
			const BulletSystemData& world_data = bulletsys->GetWorldData();
			btVector3 grav = !world_data.GetGravity();
			if(btDynamicsWorld *world = bulletsys->GetDynamicsWorld()){
				if( mRigidBody ){
					world->removeRigidBody(mRigidBody);
					delete mRigidBody;
					mRigidBody = nullptr;
				}
			}
	}
}
bool BulletObjectControllerInst::DoLink(Simulation* psi)
{
	if( mBOCD._disablePhysics )
		return true;

	if( auto bulletsys = psi->findSystem<BulletSystem>() ){

			auto this_ent = GetEntity();

			const BulletSystemData& world_data = bulletsys->GetWorldData();
			btVector3 grav = !world_data.GetGravity();

			if(btDynamicsWorld *world = bulletsys->GetDynamicsWorld()){
				const BulletShapeBaseData* shapedata = mBOCD.GetShapeData();

				//printf( "SHAPEDATA<%p>\n", shapedata );
				if( shapedata ){
					ShapeCreateData shape_create_data;
					shape_create_data.mEntity = GetEntity();
					shape_create_data.mWorld = bulletsys;
					shape_create_data.mObject = this;

					mShapeInst = shapedata->CreateShape(shape_create_data);

					btCollisionShape* pshape = mShapeInst->mCollisionShape;

					if( pshape ){
						////////////////////////////////
						const DagNode& dnode = shape_create_data.mEntity->GetEntData().GetDagNode();
						const TransformNode& t3d = dnode.GetTransformNode();
						fmtx4 mtx = t3d.GetTransform().GetMatrix();
						////////////////////////////////
						btTransform btTrans = ! mtx;
						mRigidBody = bulletsys->AddLocalRigidBody(shape_create_data.mEntity,mBOCD.GetMass(),btTrans,pshape);
						mRigidBody->setGravity(grav);
						bool ballowsleep = mBOCD.GetAllowSleeping();
                        if(mBOCD.GetKinematic())
                        {
                            mRigidBody->setCollisionFlags( btCollisionObject::CF_KINEMATIC_OBJECT );
                            ballowsleep = false;
                        }
						mRigidBody->setActivationState(ballowsleep?WANTS_DEACTIVATION:DISABLE_DEACTIVATION);
						mRigidBody->activate();
						//orkprintf( "mRigidBody<%p>\n", mRigidBody );
					}
				}



				for( orkmap<PoolString,BulletObjectForceControllerInst*>::const_iterator
						it=mForceControllerInstMap.begin();
						it!=mForceControllerInstMap.end();
						it++ ){
					BulletObjectForceControllerInst* forcecontroller = it->second;
					//printf( "forcecontroller<%p>\n", forcecontroller );
					if( forcecontroller ){
						forcecontroller->DoLink(psi);
					}
				}

				bulletsys->LinkPhysicsObject(psi, this_ent);

			}
	} // if( auto bulletsys = psi->findSystem<BulletSystem>() ){
	return true;
}

void BulletObjectControllerInst::DoUpdate(ork::ent::Simulation* inst)
{
	if( mBOCD._disablePhysics )
		return;

	if( mRigidBody )
	{
		//////////////////////////////////////////////////
		// copy motion state to entity transform
		//////////////////////////////////////////////////

		DagNode& dnode = GetEntity()->GetDagNode();
		TransformNode& t3d = dnode.GetTransformNode();

        btTransform xf;
        if(mBOCD.GetKinematic())
        {
            btMotionState* motionState = mRigidBody->getMotionState();
            auto ork_mtx = t3d.GetTransform().GetMatrix();
            xf = ! ork_mtx;
            motionState->setWorldTransform(xf);
        }
        else
        {
            const btMotionState* motionState = mRigidBody->getMotionState();
            motionState->getWorldTransform(xf);
            fmtx4 xfW = ! xf;
            t3d.GetTransform().SetMatrix( xfW );
        }

		//////////////////////////////////////////////////
		// update dynamic rigid body params
		//////////////////////////////////////////////////

		mRigidBody->setRestitution(mBOCD.GetRestitution());
		mRigidBody->setFriction(mBOCD.GetFriction());
		mRigidBody->setDamping( mBOCD.GetLinearDamping(), mBOCD.GetAngularDamping() );

		//////////////////////////////////////////////////
		// apply forces
		//////////////////////////////////////////////////

		for( auto item : mForceControllerInstMap ){
			auto forcecontroller = item.second;
			if( forcecontroller )
				forcecontroller->UpdateForces(inst,this);
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
	: mShapeFactory(nullptr)
{
}
BulletShapeBaseData::~BulletShapeBaseData()
{
}

BulletShapeBaseInst* BulletShapeBaseData::CreateShape(const ShapeCreateData& data) const
{
	BulletShapeBaseInst* rval = nullptr;

	if( mShapeFactory._createShape )
	{
		rval = mShapeFactory._createShape(data);

		if( rval && rval->mCollisionShape )
		{
			btTransform ident;
			ident.setIdentity();

			btVector3 bbmin;
			btVector3 bbmax;

			rval->mCollisionShape->getAabb( ident, bbmin, bbmax );
			rval->mBoundingBox.SetMinMax( ! bbmin, ! bbmax );
		}

	}

	OrkAssert(rval);

	return rval;
}

bool BulletShapeBaseData::DoNotify(const event::Event *event)
{
	if( const ObjectGedEditEvent* pgev = rtti::autocast(event) )
	{
        UpdateSerialOpQ().push(Op([this](){
            mShapeFactory._invalidate(this);
		}));
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeCapsuleData::Describe()
{
	reflect::RegisterFloatMinMaxProp( &BulletShapeCapsuleData::mfExtent, "Extent", "0.1", "1000.0" );
	reflect::RegisterFloatMinMaxProp( &BulletShapeCapsuleData::mfRadius, "Radius", "0.1", "1000.0" );
}

BulletShapeCapsuleData::BulletShapeCapsuleData()
	: mfRadius(0.10f)
	, mfExtent(1.0f)
{
	mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst*
	{
		auto rval = new BulletShapeBaseInst;
		rval->mCollisionShape = new btCapsuleShapeZ( this->mfRadius, this->mfExtent );
		return rval;
	};
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapePlaneData::Describe()
{
}

BulletShapePlaneData::BulletShapePlaneData()
{
	mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst*
	{
		auto rval = new BulletShapeBaseInst;
		rval->mCollisionShape = new btStaticPlaneShape(  ! fvec3::Green(), 0.0f );
		return rval;
	};
}

///////////////////////////////////////////////////////////////////////////////////////

void BulletShapeSphereData::Describe()
{
	ork::reflect::RegisterFloatMinMaxProp( &BulletShapeSphereData::mfRadius, "Radius", "0.1", "1000.0" );
}

BulletShapeSphereData::BulletShapeSphereData()
	: mfRadius(1.0f)
{
	mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst*
	{
		auto rval = new BulletShapeBaseInst;
		rval->mCollisionShape = new btSphereShape(  this->mfRadius );
		return rval;
	};
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
	mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst*
	{
		auto rval = new BulletShapeBaseInst;
		if( mModelAsset )
		{	const lev2::XgmModel* model = this->mModelAsset->GetModel();
			if( model && model->GetNumMeshes() )
				rval->mCollisionShape = XgmModelToGimpactShape(model,this->mfScale);
		}
		else
			rval->mCollisionShape = new btSphereShape(this->mfScale);
		return rval;
	};
}

BulletShapeModelData::~BulletShapeModelData()
{
}

///////////////////////////////////////////////////////////////////////////////
void BulletShapeModelData::SetModelAccessor( ork::rtti::ICastable* const & mdl)
{	mModelAsset = mdl ? ork::rtti::autocast( mdl ) : 0;
}
void BulletShapeModelData::GetModelAccessor( ork::rtti::ICastable* & mdl) const
{	mdl = mModelAsset;
}

} } // namespace ork { namespace ent
