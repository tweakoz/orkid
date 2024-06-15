////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector3.h>
#include <ork/math/quaternion.h>
#include <ork/math/cmatrix4.h>
#include <ork/kernel/tempstring.h>
#include <ork/rtti/RTTIX.inl>
#include <ork/object/Object.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

struct TransformNode;
struct DecompTransform;
using decompxf_ptr_t = std::shared_ptr<DecompTransform>;
using xfnode_ptr_t = std::shared_ptr<TransformNode>;
using decompxf_const_ptr_t = std::shared_ptr<const DecompTransform>;
using xfnode_const_ptr_t = std::shared_ptr<const TransformNode>;

///////////////////////////////////////////////////////////////////////////////

struct DecompTransform : public ork::Object {

  DeclareConcreteX(DecompTransform, ork::Object);

  DecompTransform();
  ~DecompTransform();

  decompxf_ptr_t _parent;
  fvec3 _translation;
  fquat _rotation;
  float _uniformScale = 1.0f;
  fvec3 _nonUniformScale;
  std::atomic<int> _state;
  
  fmtx4 _directmatrix;
  bool _usedirectmatrix = false;
  bool _viewRelative = false;
  bool _useNonUniformScale = false;

  void set(fvec3 t, fquat r, float s) { _translation=t; _rotation=r; _uniformScale=s; }
  void set(decompxf_const_ptr_t rhs);

  fmtx4 composed() const;
  fmtx4 composed2() const;
  void decompose( const fmtx4& inmtx );
};

///////////////////////////////////////////////////////////////////////////////

struct TransformNode : public ork::Object {

  DeclareConcreteX(TransformNode, ork::Object);

public:

  TransformNode();
  TransformNode(const TransformNode& oth);
  ~TransformNode();

  const TransformNode& operator=(const TransformNode& oth);
  bool operator==(const TransformNode& rhs) const;
  bool operator!=(const TransformNode& rhs) const;

  fmtx4 computeMatrix() const;

  xfnode_const_ptr_t _parent;
  decompxf_ptr_t _transform;

};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork
