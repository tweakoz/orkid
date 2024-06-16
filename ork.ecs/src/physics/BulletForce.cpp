////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/orklut.hpp>
#include <ork/kernel/opq.h>

#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>

#include <ork/math/basicfilters.h>
#include <ork/math/misc_math.h>

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>

#include <ork/ecs/entity.h>
#include <ork/ecs/scene.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/physics/bullet.h>

#include "bullet_impl.h"

ImplementReflectionX(ork::ecs::BulletObjectForceControllerData, "BulletObjectForceControllerData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectForceControllerData::describeX(object::ObjectClass* clazz) {
}

BulletObjectForceControllerData::BulletObjectForceControllerData() {
}
BulletObjectForceControllerData::~BulletObjectForceControllerData() {
}

BulletObjectForceControllerInst::BulletObjectForceControllerInst(){
}
BulletObjectForceControllerInst::~BulletObjectForceControllerInst() {
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

struct TestForceControllerData : public BulletObjectForceControllerData {
  DeclareConcreteX(TestForceControllerData, BulletObjectForceControllerData);

public:
  TestForceControllerData()
      : mForce(1.0f)
      , mTorque(0.1f)
      , mOrigin(0.0f, 0.0f, 0.0f)
      , mPFACTOR(0.0f)
      , mIFACTOR(0.0f)
      , mDFACTOR(0.0f)
      , mfErrPower(1.0f) {
  }
  ~TestForceControllerData() {
  }

  BulletObjectForceControllerInst*
  CreateForceControllerInst(const BulletObjectComponentData& data, Entity* pent) const final;

  float mForce;
  float mTorque;
  fvec3 mOrigin;
  float mPFACTOR;
  float mIFACTOR;
  float mDFACTOR;
  PoolString mTarget;
  float mfErrPower;
};

struct MyPid {
  MyPid()
      : mLastError(0.0f)
      , mIntegral(0.0f)
      , mProportionalFactor(0.075f)
      , mIntegralFactor(0.0f)
      , mDerivativeFactor(0.05f)
      , mIntegralMin(-10.0f)
      , mIntegralMax(+10.0f)
      , mDeltaMin(-1000.0f)
      , mDeltaMax(+1000.0f)
      , mIntegralDecay(0.9f) {
  }

  float Update(float MeasuredError, float dt) {
    mIntegral += MeasuredError * dt;
    mIntegral *= powf(mIntegralDecay, dt);
    mIntegral    = maximum(mIntegral, mIntegralMin);
    mIntegral    = minimum(mIntegral, mIntegralMax);
    float P      = MeasuredError;
    float D      = (MeasuredError - mLastError) / dt;
    float output = P * mProportionalFactor + mIntegral * mIntegralFactor + D * mDerivativeFactor;
    //////////////////////
    // clamp output
    output = (output < mDeltaMin) ? mDeltaMin : output;
    output = (output > mDeltaMax) ? mDeltaMax : output;
    //////////////////////
    mLastError = MeasuredError;
    return output;
  }
  void Configure(float P, float I, float D) {
    mProportionalFactor = P;
    mIntegralFactor     = I;
    mDerivativeFactor   = D;
  }

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

struct TestForceControllerInst final : public BulletObjectForceControllerInst {
  TestForceControllerInst(const TestForceControllerData& data);
  ~TestForceControllerInst();
  void UpdateForces(BulletObjectComponent* boci, float deltat) final;
  bool DoLink(Simulation* psi) final;

  MyPid mPIDsteering;
  MyPid mPIDroll;
  Entity* _target;
  const TestForceControllerData& mTestData;
};

///////////////////////////////////////////////////////////////////////////////

void TestForceControllerData::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty( "Force", float_range{-80.0, 80.0}, &TestForceControllerData::mForce);
  clazz->floatProperty( "Torque", float_range{0.0, 100.0}, &TestForceControllerData::mTorque);
  clazz->floatProperty( "PFactor", float_range{-1.0, 1.0}, &TestForceControllerData::mPFACTOR);
  clazz->floatProperty( "IFactor", float_range{-1.0, 1.0}, &TestForceControllerData::mIFACTOR);
  clazz->floatProperty( "DFactor", float_range{-1.0, 1.0}, &TestForceControllerData::mDFACTOR);
  clazz->floatProperty( "ErrPower", float_range{0.01, 100.0}, &TestForceControllerData::mfErrPower);

  clazz->directProperty("Target", &TestForceControllerData::mTarget);
  clazz->directProperty("Origin", &TestForceControllerData::mOrigin);
}

BulletObjectForceControllerInst*
TestForceControllerData::CreateForceControllerInst(const BulletObjectComponentData& data, Entity* pent) const {
  TestForceControllerInst* rval = new TestForceControllerInst(*this);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

TestForceControllerInst::TestForceControllerInst(const TestForceControllerData& data)
    : _target(0)
    , mTestData(data) {
}
TestForceControllerInst::~TestForceControllerInst() {
}

bool TestForceControllerInst::DoLink(Simulation* psi) {
  _target = psi->findEntity(mTestData.mTarget);
  return true;
}

void TestForceControllerInst::UpdateForces(BulletObjectComponent* boci, float deltat) {
  const BulletObjectComponentData& BOCD = boci->data();
  btRigidBody* rbody                     = boci->_rigidbody;
  const btMotionState* motionState       = rbody->getMotionState();

  float FORCE  = mTestData.mForce;
  fvec3 ORIGIN = mTestData.mOrigin;
  mPIDsteering.Configure(mTestData.mPFACTOR, mTestData.mIFACTOR, mTestData.mDFACTOR);
  mPIDroll.Configure(mTestData.mPFACTOR, mTestData.mIFACTOR, mTestData.mDFACTOR);

  /////////////////////////////

  if (_target) {
    auto tgt_xform     = _target->transform();
    ORIGIN             = tgt_xform->_translation;
  }

  // printf( "target<%f %f %f\n",
  //	ORIGIN.x, ORIGIN.y, ORIGIN.z );

  /////////////////////////////
  btTransform xf;
  motionState->getWorldTransform(xf);
  fmtx4 xfW           = btmtx4toorkmtx4(xf);
  fvec3 pos           = btv3toorkv3(xf.getOrigin());
  fvec3 znormal       = xfW.zNormal();
  fvec3 xnormal       = xfW.xNormal();
  fvec3 ynormal       = xfW.yNormal();
  fvec3 dir_to_origin = (ORIGIN - pos).normalized();
  /////////////////////////////

  auto shape_inst = boci->_shapeinst;

  const AABox& bbox = shape_inst->GetBoundingBox();
  fvec3 ctr         = (bbox.Min() + bbox.Max()) * 0.5f;
  fvec3 ctr_bak(ctr.x, ctr.y, bbox.Max().z);
  fvec3 force_pos = pos; // - ctr_bak;
  fvec3 force_dir = dir_to_origin;
  fvec3 force_amt = force_dir * FORCE;
  // rbody->applyForce( ! force_amt, ! force_pos );
  rbody->applyCentralForce(orkv3tobtv3(force_amt));
  /////////////////////////////
  // Get Direction to Target

  /////////////////////////////
  // Get Torque Axis

  fvec3 Z_torque_vec = znormal.crossWith(dir_to_origin);

  ///////////////////////////////
  // torque to for ROLL
  {
    fvec3 Y_reference = Z_torque_vec.crossWith(dir_to_origin);
    /////////////////////////////
    // the Z_torque_ref_plane is the plane
    //
    fplane3 Z_torque_ref_plane;
    Z_torque_ref_plane.CalcFromNormalAndOrigin(Z_torque_vec, pos);
    float ztrpD = Z_torque_ref_plane.pointDistance(ORIGIN);
    /////////////////////////////
    // Absolute Error
    float Y_fDOT    = ynormal.dotWith(Z_torque_vec); // 1 when we are heading to it, -1 when heading away
    float Y_ferrABS = fabs((1.0f - Y_fDOT) * 0.5f);

    if (Y_ferrABS > 0.001f) {
      Y_ferrABS = powf(Y_ferrABS, mTestData.mfErrPower);
    }
    /////////////////////////////
    // Splitting plane for signed error
    fvec3 Y_split_vec = ynormal.crossWith(Z_torque_vec);
    fplane3 Y_plane;
    Y_plane.CalcFromNormalAndOrigin(Z_torque_vec, pos); //! calc given normal and position of plane origin
    /////////////////////////////
    // Signed Error
    float Y_fdistfromsplitplane = Z_torque_ref_plane.pointDistance(pos + ynormal);
    float Y_fsign               = (Y_fdistfromsplitplane < 0.0f) ? 1.0f : -1.0f;
    float Y_ferr                = Y_ferrABS * Y_fsign;
    /////////////////////////////
    float Y_famt = mPIDroll.Update(Y_ferr, 1.0f);
    float kfmaxT = std::clamp(mTestData.mTorque * 0.1f, -1.0f, 1.0f);
    auto torque = orkv3tobtv3(znormal * Y_famt);
    rbody->applyTorque(torque);
    /////////////////////////////
  }

  ///////////////////////////////
  // Z torque
  {
    /////////////////////////////
    // Get Torque Axis
    /////////////////////////////
    // Absolute Error
    float Z_fDOT    = znormal.dotWith(dir_to_origin); // 1 when we are heading to it, -1 when heading away
    float Z_ferrABS = fabs((1.0f - Z_fDOT) * 0.5f);

    if (Z_ferrABS > 0.001f) {
      Z_ferrABS = powf(Z_ferrABS, mTestData.mfErrPower);
    }
    /////////////////////////////
    // Splitting plane for signed error
    fvec3 Z_split_vec = znormal.crossWith(Z_torque_vec);
    fplane3 Z_plane;
    Z_plane.CalcFromNormalAndOrigin(Z_split_vec, pos); //! calc given normal and position of plane origin
    /////////////////////////////
    // Signed Error
    float Z_fdistfromsplitplane = Z_plane.pointDistance(ORIGIN);
    float Z_fsign               = (Z_fdistfromsplitplane < 0.0f) ? 1.0f : -1.0f;
    float Z_ferr                = Z_ferrABS * Z_fsign;
    /////////////////////////////
    float Z_famt = mPIDsteering.Update(Z_ferr, 1.0f);
    if (Z_famt > mTestData.mTorque)
      Z_famt = mTestData.mTorque;
    if (Z_famt < -mTestData.mTorque)
      Z_famt = -mTestData.mTorque;
    auto torque = orkv3tobtv3(Z_torque_vec * Z_famt);
    rbody->applyTorque(torque);
    // printf( "rbody<%p> ZTORQUE<%f>\n", rbody, Z_famt );
    /////////////////////////////
  }
  //		float famt = (2.0f-(1.0f+fdot));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


void DirectionalForceData::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("Force", float_range{-1000,1000}, &DirectionalForceData::_force);
  clazz->directProperty("Direction", &DirectionalForceData::_direction);
}

BulletObjectForceControllerInst*
DirectionalForceData::CreateForceControllerInst(const BulletObjectComponentData& data, Entity* pent) const {
  DirectionalForceInst* rval = new DirectionalForceInst(this);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

DirectionalForceInst::DirectionalForceInst(const DirectionalForceData* data) : _DFD(data) {
}
DirectionalForceInst::~DirectionalForceInst() {
}

bool DirectionalForceInst::DoLink(Simulation* psi) {
  // _target = psi->FindEntity(mTestData.GetTarget());
  return true;
}

void DirectionalForceInst::UpdateForces(BulletObjectComponent* boci, float deltat) {
  const BulletObjectComponentData& BOCD = boci->data();
  btRigidBody* rbody                     = boci->_rigidbody;

  fvec3 force_amt = _DFD->_direction * _DFD->_force;
  rbody->applyCentralForce(orkv3tobtv3(force_amt));
  
  //printf( "apply force<%g %g %g>\n", force_amt.x, force_amt.y, force_amt.z );
  /////////////////////////////
  /////////////////////////////
  //const btMotionState* motionState       = rbody->getMotionState();
  //btTransform xf;
  //motionState->getWorldTransform(xf);
  //fmtx4 xfW     = btmtx4toorkmtx4(xf);
  //fvec3 pos     = btv3toorkv3(xf.getOrigin());
  //fvec3 znormal = xfW.zNormal();
  //fvec3 xnormal = xfW.xNormal();
  //fvec3 ynormal = xfW.yNormal();
  //auto shape_inst   = boci->_shapeinst;
  //const AABox& bbox = shape_inst->GetBoundingBox();
  //fvec3 ctr = (bbox.Min() + bbox.Max()) * 0.5f;
  //fvec3 ctr_bak(ctr.x, ctr.y, bbox.Max().z);
  //fvec3 force_pos = pos; // - ctr_bak;
  // rbody->applyForce( ! force_amt, ! force_pos );
  /////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////

ImplementReflectionX(ork::ecs::TestForceControllerData, "TestForceControllerData");
ImplementReflectionX(ork::ecs::DirectionalForceData, "DirectionalForceData");
