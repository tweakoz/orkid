////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <BulletCollision/CollisionShapes/btStaticPlaneShape.h>
#include <BulletCollision/Gimpact/btGImpactCollisionAlgorithm.h>
#include <BulletCollision/Gimpact/btGImpactShape.h>
#include <Extras/GIMPACTUtils/btGImpactConvexDecompositionShape.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <pkg/ent/bullet.h>

#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>

#include <ork/kernel/orklut.hpp>
#include <ork/math/basicfilters.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

#include <ork/math/PIDController.h>

#include <ork/kernel/opq.h>
#include <pkg/ent/scene.hpp>
#include <ork/math/misc_math.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletObjectForceControllerData, "BulletObjectForceControllerData");

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////

void BulletObjectForceControllerData::Describe() {}

BulletObjectForceControllerData::BulletObjectForceControllerData() {}
BulletObjectForceControllerData::~BulletObjectForceControllerData() {}

BulletObjectForceControllerInst::BulletObjectForceControllerInst(const BulletObjectForceControllerData& data) : mData(data) {}
BulletObjectForceControllerInst::~BulletObjectForceControllerInst() {}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

struct TestForceControllerData : public BulletObjectForceControllerData {
  RttiDeclareConcrete(TestForceControllerData, BulletObjectForceControllerData);

public:
  TestForceControllerData()
      : mForce(1.0f), mTorque(0.1f), mOrigin(0.0f, 0.0f, 0.0f), mPFACTOR(0.0f), mIFACTOR(0.0f), mDFACTOR(0.0f), mfErrPower(1.0f) {}
  ~TestForceControllerData() {}

  BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectControllerData& data,
                                                             ork::ent::Entity* pent) const final;

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
      : mProportionalFactor(0.075f), mIntegralFactor(0.0f), mDerivativeFactor(0.05f), mIntegralMin(-10.0f), mIntegralMax(+10.0f),
        mDeltaMin(-1000.0f), mDeltaMax(+1000.0f), mLastError(0.0f), mIntegral(0.0f), mIntegralDecay(0.9f) {}

  float Update(float MeasuredError, float dt) {
    mIntegral += MeasuredError * dt;
    mIntegral *= powf(mIntegralDecay, dt);
    mIntegral = maximum(mIntegral, mIntegralMin);
    mIntegral = minimum(mIntegral, mIntegralMax);
    float P = MeasuredError;
    float D = (MeasuredError - mLastError) / dt;
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
    mIntegralFactor = I;
    mDerivativeFactor = D;
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

struct TestForceControllerInst : public BulletObjectForceControllerInst {
  TestForceControllerInst(const TestForceControllerData& data);
  ~TestForceControllerInst();
  void UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci);
  bool DoLink(SceneInst* psi);

  MyPid mPIDsteering;
  MyPid mPIDroll;
  Entity* mpTarget;
  const TestForceControllerData& mTestData;
};

///////////////////////////////////////////////////////////////////////////////

void TestForceControllerData::Describe() {
  reflect::RegisterFloatMinMaxProp(&TestForceControllerData::mForce, "Force", "-80.0", "80.0");
  reflect::RegisterFloatMinMaxProp(&TestForceControllerData::mTorque, "Torque", "0.0", "100.0");
  reflect::RegisterFloatMinMaxProp(&TestForceControllerData::mPFACTOR, "PFactor", "-1.0", "1.0");
  reflect::RegisterFloatMinMaxProp(&TestForceControllerData::mIFACTOR, "IFactor", "-1.0", "1.0");
  reflect::RegisterFloatMinMaxProp(&TestForceControllerData::mDFACTOR, "DFactor", "-1.0", "1.0");
  reflect::RegisterFloatMinMaxProp(&TestForceControllerData::mfErrPower, "ErrPower", "0.01", "100.0");

  reflect::RegisterProperty("Target", &TestForceControllerData::mTarget);
  reflect::RegisterProperty("Origin", &TestForceControllerData::mOrigin);
}

BulletObjectForceControllerInst* TestForceControllerData::CreateForceControllerInst(const BulletObjectControllerData& data,
                                                                                    ork::ent::Entity* pent) const {
  TestForceControllerInst* rval = new TestForceControllerInst(*this);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

TestForceControllerInst::TestForceControllerInst(const TestForceControllerData& data)
    : BulletObjectForceControllerInst(data), mTestData(data), mpTarget(0) {}
TestForceControllerInst::~TestForceControllerInst() {}

bool TestForceControllerInst::DoLink(SceneInst* psi) {
  mpTarget = psi->FindEntity(mTestData.mTarget);
  return true;
}

void TestForceControllerInst::UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci) {
  float fDT = inst->GetDeltaTime();
  const BulletObjectControllerData& BOCD = boci->GetData();
  btRigidBody* rbody = boci->GetRigidBody();
  const btMotionState* motionState = rbody->getMotionState();

  float FORCE = mTestData.mForce;
  fvec3 ORIGIN = mTestData.mOrigin;
  mPIDsteering.Configure(mTestData.mPFACTOR, mTestData.mIFACTOR, mTestData.mDFACTOR);
  mPIDroll.Configure(mTestData.mPFACTOR, mTestData.mIFACTOR, mTestData.mDFACTOR);

  /////////////////////////////

  if (mpTarget) {
    DagNode& dnode = mpTarget->GetDagNode();
    TransformNode& t3d = dnode.GetTransformNode();
    fmtx4 mtx = t3d.GetTransform().GetMatrix();
    ORIGIN = mtx.GetTranslation();
  }

  // printf( "target<%f %f %f\n",
  //	ORIGIN.GetX(), ORIGIN.GetY(), ORIGIN.GetZ() );

  /////////////////////////////
  btTransform xf;
  motionState->getWorldTransform(xf);
  fmtx4 xfW = !xf;
  fvec3 pos = !xf.getOrigin();
  fvec3 znormal = xfW.GetZNormal();
  fvec3 xnormal = xfW.GetXNormal();
  fvec3 ynormal = xfW.GetYNormal();
  fvec3 dir_to_origin = (ORIGIN - pos).Normal();
  /////////////////////////////

  auto shape_inst = boci->GetShapeInst();

  const AABox& bbox = shape_inst->GetBoundingBox();
  fvec3 ctr = (bbox.Min() + bbox.Max()) * 0.5f;
  fvec3 ctr_bak(ctr.GetX(), ctr.GetY(), bbox.Max().GetZ());
  fvec3 force_pos = pos; // - ctr_bak;
  fvec3 force_dir = dir_to_origin;
  fvec3 force_amt = force_dir * FORCE;
  // rbody->applyForce( ! force_amt, ! force_pos );
  rbody->applyCentralForce(!force_amt);
  /////////////////////////////
  // Get Direction to Target

  /////////////////////////////
  // Get Torque Axis

  fvec3 Z_torque_vec = znormal.Cross(dir_to_origin);

  ///////////////////////////////
  // torque to for ROLL
  {
    fvec3 Y_reference = Z_torque_vec.Cross(dir_to_origin);
    /////////////////////////////
    // the Z_torque_ref_plane is the plane
    //
    fplane3 Z_torque_ref_plane;
    Z_torque_ref_plane.CalcFromNormalAndOrigin(Z_torque_vec, pos);
    float ztrpD = Z_torque_ref_plane.GetPointDistance(ORIGIN);
    /////////////////////////////
    // Absolute Error
    float Y_fDOT = ynormal.Dot(Z_torque_vec); // 1 when we are heading to it, -1 when heading away
    float Y_ferrABS = fabs((1.0f - Y_fDOT) * 0.5f);

    if (Y_ferrABS > 0.001f) {
      Y_ferrABS = powf(Y_ferrABS, mTestData.mfErrPower);
    }
    /////////////////////////////
    // Splitting plane for signed error
    fvec3 Y_split_vec = ynormal.Cross(Z_torque_vec);
    fplane3 Y_plane;
    Y_plane.CalcFromNormalAndOrigin(Z_torque_vec, pos); //! calc given normal and position of plane origin
    /////////////////////////////
    // Signed Error
    float Y_fdistfromsplitplane = Z_torque_ref_plane.GetPointDistance(pos + ynormal);
    float Y_fsign = (Y_fdistfromsplitplane < 0.0f) ? 1.0f : -1.0f;
    float Y_ferr = Y_ferrABS * Y_fsign;
    /////////////////////////////
    float Y_famt = mPIDroll.Update(Y_ferr, 1.0f);
    float kfmaxT = std::clamp(mTestData.mTorque * 0.1f,
                              -kfmaxT,
                              +kfmaxT);
    rbody->applyTorque(!(znormal * Y_famt));
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
    float Z_ferrABS = fabs((1.0f - Z_fDOT) * 0.5f);

    if (Z_ferrABS > 0.001f) {
      Z_ferrABS = powf(Z_ferrABS, mTestData.mfErrPower);
    }
    /////////////////////////////
    // Splitting plane for signed error
    fvec3 Z_split_vec = znormal.Cross(Z_torque_vec);
    fplane3 Z_plane;
    Z_plane.CalcFromNormalAndOrigin(Z_split_vec, pos); //! calc given normal and position of plane origin
    /////////////////////////////
    // Signed Error
    float Z_fdistfromsplitplane = Z_plane.GetPointDistance(ORIGIN);
    float Z_fsign = (Z_fdistfromsplitplane < 0.0f) ? 1.0f : -1.0f;
    float Z_ferr = Z_ferrABS * Z_fsign;
    /////////////////////////////
    float Z_famt = mPIDsteering.Update(Z_ferr, 1.0f);
    if (Z_famt > mTestData.mTorque)
      Z_famt = mTestData.mTorque;
    if (Z_famt < -mTestData.mTorque)
      Z_famt = -mTestData.mTorque;
    rbody->applyTorque(!(Z_torque_vec * Z_famt));
    // printf( "rbody<%p> ZTORQUE<%f>\n", rbody, Z_famt );
    /////////////////////////////
  }
  //		float famt = (2.0f-(1.0f+fdot));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class DirectionalForceData : public BulletObjectForceControllerData {
  RttiDeclareConcrete(DirectionalForceData, BulletObjectForceControllerData);

public:
  DirectionalForceData() : mForce(1.0f), mDirection(0.0f, 0.0f, 0.0f) {}

  float GetForce() const { return mForce; }
  const fvec3& GetDirection() const { return mDirection; }

private:
  ~DirectionalForceData() final {}
  BulletObjectForceControllerInst* CreateForceControllerInst(const BulletObjectControllerData& data,
                                                             ork::ent::Entity* pent) const final;

  float mForce;
  fvec3 mDirection;
};

///////////////////////////////////////////////////////////////////////////////

class DirectionalForceInst : public BulletObjectForceControllerInst {
public:
  DirectionalForceInst(const DirectionalForceData& data);
  ~DirectionalForceInst();
  void UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci);
  bool DoLink(SceneInst* psi);

private:
  const DirectionalForceData& mData;
};

void DirectionalForceData::Describe() {
  reflect::RegisterProperty("Force", &DirectionalForceData::mForce);
  reflect::RegisterProperty("Direction", &DirectionalForceData::mDirection);
  reflect::AnnotatePropertyForEditor<DirectionalForceData>("Force", "editor.range.min", "-1000.0");
  reflect::AnnotatePropertyForEditor<DirectionalForceData>("Force", "editor.range.max", "1000.0");
}

BulletObjectForceControllerInst* DirectionalForceData::CreateForceControllerInst(const BulletObjectControllerData& data,
                                                                                 ork::ent::Entity* pent) const {
  DirectionalForceInst* rval = new DirectionalForceInst(*this);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

DirectionalForceInst::DirectionalForceInst(const DirectionalForceData& data) : BulletObjectForceControllerInst(data), mData(data) {}
DirectionalForceInst::~DirectionalForceInst() {}

bool DirectionalForceInst::DoLink(SceneInst* psi) {
  // mpTarget = psi->FindEntity(mTestData.GetTarget());
  return true;
}

void DirectionalForceInst::UpdateForces(ork::ent::SceneInst* inst, BulletObjectControllerInst* boci) {
  float fDT = inst->GetDeltaTime();
  const BulletObjectControllerData& BOCD = boci->GetData();
  btRigidBody* rbody = boci->GetRigidBody();
  const btMotionState* motionState = rbody->getMotionState();

  float FORCE = mData.GetForce();
  fvec3 DIRECTION = mData.GetDirection();

  /////////////////////////////
  btTransform xf;
  motionState->getWorldTransform(xf);
  fmtx4 xfW = !xf;
  fvec3 pos = !xf.getOrigin();
  fvec3 znormal = xfW.GetZNormal();
  fvec3 xnormal = xfW.GetXNormal();
  fvec3 ynormal = xfW.GetYNormal();
  /////////////////////////////
  auto shape_inst = boci->GetShapeInst();
  const AABox& bbox = shape_inst->GetBoundingBox();

  fvec3 ctr = (bbox.Min() + bbox.Max()) * 0.5f;
  fvec3 ctr_bak(ctr.GetX(), ctr.GetY(), bbox.Max().GetZ());
  fvec3 force_pos = pos; // - ctr_bak;
  fvec3 force_amt = DIRECTION * FORCE;
  // rbody->applyForce( ! force_amt, ! force_pos );
  rbody->applyCentralForce(!force_amt);
  /////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::TestForceControllerData, "TestForceControllerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::DirectionalForceData, "DirectionalForceData");
