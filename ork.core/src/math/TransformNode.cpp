////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/math/TransformNode.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTyped.hpp>

ImplementReflectionX(ork::TransformNode, "TransformNode");
ImplementReflectionX(ork::DecompTransform, "DecompTransform");

namespace ork {


///////////////////////////////////////////////////////////////////////////////
void DecompTransform::describeX(class_t* c) {
  c->directProperty("translation", &DecompTransform::_translation);
  c->directProperty("rotation", &DecompTransform::_rotation);
  c->directProperty("uniformScale", &DecompTransform::_uniformScale);
}
///////////////////////////////////////////////////////////////////////////////
DecompTransform::DecompTransform(){
  _state.store(100);
}
///////////////////////////////////////////////////////////////////////////////
DecompTransform::~DecompTransform(){
  _state.store(0);
}
void DecompTransform::set(decompxf_const_ptr_t rhs){
    OrkAssert(rhs->_state.load()!=0);
    OrkAssert(_state.load()!=0);
    _translation = rhs->_translation;
    _rotation = rhs->_rotation;
    _uniformScale = rhs->_uniformScale;
}
///////////////////////////////////////////////////////////////////////////////

fmtx4 DecompTransform::composed() const{
  fmtx4 rval;

  if(_usedirectmatrix){
    rval = _directmatrix;
  }
  else{
    if(_useNonUniformScale){
      float x = _nonUniformScale.x;
      float y = _nonUniformScale.y;
      float z = _nonUniformScale.z;
      rval.compose(_translation,_rotation,x,y,z);
    }
    else{
      rval.compose(_translation,_rotation,_uniformScale);
    }
  }
  return rval;
}

fmtx4 DecompTransform::composed2() const{
  fmtx4 rval;
  if(_usedirectmatrix){
    rval = _directmatrix;
  }
  else{
    if(_useNonUniformScale){
      float x = _nonUniformScale.x;
      float y = _nonUniformScale.y;
      float z = _nonUniformScale.z;
      rval.compose2(_translation,_rotation,x,y,z);
    }
    else{
      rval.compose2(_translation,_rotation,_uniformScale);
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void DecompTransform::decompose( const fmtx4& inmtx ){
  inmtx.decompose(_translation,_rotation,_uniformScale);
  //auto mtxstr = inmtx.dump4x3cn();
  //printf( " xfval<%s> t<%g %g %g> r<%g %g %g %g> \n", mtxstr.c_str(), _translation.x, _translation.y, _translation.z, _rotation.x, _rotation.y, _rotation.z, _rotation.w );
}

///////////////////////////////////////////////////////////////////////////////
void TransformNode::describeX(class_t* c) {
  c->directObjectProperty("transform", &TransformNode::_transform);
}

///////////////////////////////////////////////////////////////////////////////

TransformNode::TransformNode()
    : _parent(nullptr) 
    , _transform( std::make_shared<DecompTransform>() ) {
}

TransformNode::TransformNode(const TransformNode& oth)
    : _parent(oth._parent) 
    , _transform( std::make_shared<DecompTransform>() ) {
    _transform->_translation = oth._transform->_translation;
    _transform->_rotation = oth._transform->_rotation;
    _transform->_uniformScale = oth._transform->_uniformScale;
}

///////////////////////////////////////////////////////////////////////////////

TransformNode::~TransformNode() {
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 TransformNode::computeMatrix() const {
  if(_parent){
    return fmtx4::multiply_ltor(_transform->composed(),_parent->_transform->composed());
  }
  else{
    return _transform->composed();
  }
}

///////////////////////////////////////////////////////////////////////////////

const TransformNode& TransformNode::operator=(const TransformNode& rhs) {
  _transform->_translation = rhs._transform->_translation;
  _transform->_rotation = rhs._transform->_rotation;
  _transform->_uniformScale = rhs._transform->_uniformScale;
  _parent = rhs._parent;
  return *this;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformNode::operator==(const TransformNode& rhs) const {

  bool match = (computeMatrix() == rhs.computeMatrix());
  match &= (_parent == rhs._parent);
  return match;
}

///////////////////////////////////////////////////////////////////////////////

bool TransformNode::operator!=(const TransformNode& rhs) const {
  return not this->operator==(rhs);
}

///////////////////////////////////////////////////////////////////////////////

/*
 * namespace reflect::serdes {
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
  tstr.format("(%g %g %g) (%g %g %g %g) (%g)", pos.x, pos.y, pos.z, quat.x, quat.y, quat.z, quat.w, scale);
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
*/
///////////////////////////////////////////////////////////////////////////////

} // namespace ork
