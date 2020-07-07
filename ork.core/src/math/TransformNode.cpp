////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/TransformNode.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

void TransformNode::CopyFrom(const TransformNode& oth) {
  mTransform.SetMatrix(oth.GetTransform().GetMatrix());
  mpParent = oth.mpParent;
}

TransformNode::TransformNode()
    : mTransform()
    , mpParent(0) {
}

///////////////////////////////////////////////////////////////////////////////

void TransformNode::GetMatrix(fmtx4& mtx) const {
  if (mpParent) {
    fmtx4 p;
    mpParent->GetMatrix(p);
    mtx = GetTransform().GetMatrix() * p;
  } else {
    mtx = GetTransform().GetMatrix();
  }
}

void TransformNode::Translate(ETransformHierMode emode, const fvec3& pos) {
  if (emode == EMODE_ABSOLUTE) {
    GetTransform().SetPosition(pos);
  }
}

TransformNode::TransformNode(const TransformNode& oth)
    : mTransform()
    , mpParent(0) {
  CopyFrom(oth);
}

const TransformNode& TransformNode::operator=(const TransformNode& oth) {
  if (this != &oth) {
    CopyFrom(oth);
  }

  return *this;
}

TransformNode::~TransformNode() {
}

///////////////////////////////////////////////////////////////////////////////

bool TransformNode::operator==(const TransformNode& rhs) const {

  bool match = (mTransform.GetMatrix() == rhs.mTransform.GetMatrix());
  match &= (mpParent == rhs.mpParent);
  return match;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformNode::operator!=(const TransformNode& rhs) const {

  bool match = (mTransform.GetMatrix() == rhs.mTransform.GetMatrix());
  match &= (mpParent == rhs.mpParent);
  return not match;
}

///////////////////////////////////////////////////////////////////////////////

void TransformNode::UnParent(void) {
  if (mpParent != NULL) {
    fmtx4 MatW;
    GetMatrix(MatW);

    ///////////////////////////////////

    mpParent = 0;

    ///////////////////////////////////
    // Translation

    Translate(EMODE_ABSOLUTE, MatW.GetTranslation());

    ///////////////////////////////////
    // Rotation

    fmtx4 MatR = MatW;
    MatR.SetTranslation(0.0f, 0.0f, 0.0f);

    fquat NewQ;
    NewQ.FromMatrix(MatR);
    GetTransform().SetRotation(NewQ);
  }
}

void TransformNode::SetParent(const TransformNode* ppar) {
  mpParent = ppar;
}
void TransformNode::ReParent(const TransformNode* ppar) {
  OrkAssertI(ppar, "Trying to set Null Parent, if trying to unparent, use the UnParent() call");

  fmtx4 MatW = GetTransform().GetMatrix();
  fmtx4 MatParentW;
  ppar->GetMatrix(MatParentW);

  if (NULL == mpParent) {
    mpParent = ppar;

    fmtx4 CorMatrix;
    CorMatrix.CorrectionMatrix(MatParentW, MatW);

    fvec3 NewPos;
    fquat NewQ;
    float NewScale;

    CorMatrix.decompose(NewPos, NewQ, NewScale);

    // ppar->GetWorldTransform()->SetRotation( NewQ );

    ///////////////////////////////////
    // Translation

    fvec3 WorldPos       = MatW.GetTranslation();
    fvec3 ParentWorldPos = MatParentW.GetTranslation();
    Translate(EMODE_ABSOLUTE, CorMatrix.GetTranslation());
  }
}

///////////////////////////////////////////////////////////////////////////////

namespace reflect::serdes {
template <> void Serialize(const TransformNode* in, TransformNode* out, BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi | in->GetTransform().GetMatrix();
  } else {
    fmtx4 result, temp;
    bidi | temp;

    ////////////////////////////////////////

    fquat oq;
    fvec3 pos;
    float Scale;
    temp.decompose(pos, oq, Scale);
    result.compose(pos, oq, Scale);

    ////////////////////////////////////////

    out->GetTransform().SetMatrix(result);
  }
}
} // namespace reflect::serdes
///////////////////////////////////////////////////////////////////////////////

template <> const EPropType PropType<TransformNode>::meType   = EPROPTYPE_TRANSFORMNODE3D;
template <> const char* PropType<TransformNode>::mstrTypeName = "TRANSFORMNODE3D";
template <> void PropType<TransformNode>::ToString(const TransformNode& node, PropTypeString& tstr) {
  const auto& xf    = node.GetTransform();
  const fvec3 pos   = xf.GetPosition();
  const fquat quat  = xf.GetRotation();
  const float scale = xf.GetScale();
  tstr.format("(%g %g %g) (%g %g %g %g) (%g)", pos.x, pos.y, pos.z, quat.x, quat.GetY(), quat.GetZ(), quat.width(), scale);
}

template <> TransformNode PropType<TransformNode>::FromString(const PropTypeString& tstr) {
  TransformNode node;
  auto& xf = node.GetTransform();
  float x, y, z;
  float qx, qy, qz, qw;
  float s;
  sscanf(tstr.c_str(), "(%g %g %g) (%g %g %g %g) (%g)", &x, &y, &z, &qx, &qy, &qz, &qw, &s);
  node.Translate(TransformNode::EMODE_ABSOLUTE, fvec3(x, y, z));
  xf.SetRotation(fquat(qx, qy, qz, qw));
  xf.SetScale(float(s));
  return node;
}

template class PropType<TransformNode>;

///////////////////////////////////////////////////////////////////////////////

const fmtx4& Transform3DMatrix::GetMatrix() const {
  return mMatrix;
}

void Transform3DMatrix::SetMatrix(const fmtx4& mat) {
  mMatrix = mat;
}

fvec3 Transform3DMatrix::GetPosition() const {
  return mMatrix.GetTranslation();
}

fquat Transform3DMatrix::GetRotation() const {
  fquat q;
  fvec3 pos;
  float Scale;
  mMatrix.decompose(pos, q, Scale);
  return q;
}

float Transform3DMatrix::GetScale() const {
  fquat q;
  fvec3 pos;
  float Scale;
  mMatrix.decompose(pos, q, Scale);
  return Scale;
}

void Transform3DMatrix::SetRotation(const fquat& nq) {
  fquat oq;
  fvec3 pos;
  float Scale;
  mMatrix.decompose(pos, oq, Scale);
  mMatrix.compose(pos, nq, Scale);
}

void Transform3DMatrix::SetScale(const float nscale) {
  fquat q;
  fvec3 pos;
  float Scale;
  mMatrix.decompose(pos, q, Scale);
  mMatrix.compose(pos, q, nscale);
}

void Transform3DMatrix::SetPosition(const fvec3& npos) {
  fquat q;
  fvec3 pos;
  float Scale;
  mMatrix.decompose(pos, q, Scale);
  mMatrix.compose(npos, q, Scale);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
