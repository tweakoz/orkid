////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_MATH_TRANSFORM_NODE_H
#define _ORK_MATH_TRANSFORM_NODE_H

#include <ork/math/cvector3.h>
#include <ork/math/quaternion.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/tempstring.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Transform3DMatrix {
  typedef fvec3 PosType;
  typedef fquat RotType;
  typedef float ScaType;
  typedef fmtx4& MatType;

  void SetMatrix(const fmtx4&);
  const fmtx4& GetMatrix() const;
  fvec3 GetPosition() const;
  fquat GetRotation() const;
  float GetScale() const;
  void SetRotation(const fquat& q);
  void SetScale(const float scale);
  void SetPosition(const fvec3& pos);

private:
  fmtx4 mMatrix; /// matrix in world space
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct TransformNode {

  TransformNode();
  TransformNode(const TransformNode& oth);
  ~TransformNode();

  const Transform3DMatrix& GetTransform() const {
    return mTransform;
  }
  Transform3DMatrix& GetTransform() {
    return mTransform;
  }

  const TransformNode& operator=(const TransformNode& oth);

  //////////////////////////////////////////////////////////////////////////////

  void GetMatrix(fmtx4& mtx) const;

  //////////////////////////////////////////////////////////////////////////////

  enum ETransformHierMode {
    EMODE_ABSOLUTE = 0,   /// Set overwrite transform node's current component
    EMODE_LOCAL_RELATIVE, /// Concatenate
  };

  void Translate(ETransformHierMode emode, const fvec3& pos);

  //////////////////////////////////////////////////////////////////////////////

  bool operator==(const TransformNode& rhs) const;
  bool operator!=(const TransformNode& rhs) const;

  //////////////////////////////////////////////////////////////////////////////
  const TransformNode* GetParent() const {
    return mpParent;
  }
  void SetParent(const TransformNode* ppar);
  //////////////////////////////////////////////////////////////////////////////
  void UnParent();
  void ReParent(const TransformNode* ppar);
  //////////////////////////////////////////////////////////////////////////////
private:
  const TransformNode* mpParent;
  Transform3DMatrix mTransform;

  void CopyFrom(const TransformNode& oth);
  //////////////////////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
#endif
