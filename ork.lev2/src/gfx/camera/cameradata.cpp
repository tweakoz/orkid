////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/string/PoolString.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>

namespace ork::lev2 {
////////////////////////////////////////////////////////////////////////////////
CameraDataLut::CameraDataLut(){
  _state.store(77);
}
CameraDataLut::~CameraDataLut(){
  //printf( "!!!Destroy CameraDataLut<%p>!!!\n", (void*) this );
  //OrkAssert(false);
  _state.store(0);
}
////////////////////////////////////////////////////////////////////////////////
cameradata_ptr_t CameraDataLut::create(std::string named){
  _state.fetch_add(1);
  auto camdat = std::make_shared<CameraData>();
  _lut[named] = camdat;
  return camdat;
}
////////////////////////////////////////////////////////////////////////////////
size_t CameraDataLut::size() const{
  return _lut.size();
}
////////////////////////////////////////////////////////////////////////////////
cameradata_constptr_t& CameraDataLut::operator[] (const std::string& named){
  auto it = _lut.find(named);
  if(it==_lut.end()){
    _lut[named] = nullptr;
    it = _lut.find(named);
  }
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////
cameradata_constptr_t CameraDataLut::find(const std::string& named) const{
  auto it = _lut.find(named);
  if(it==_lut.end()){
    return nullptr;
  }
  return it->second;
}
////////////////////////////////////////////////////////////////////////////////
void CameraDataLut::clear(){
  _state.fetch_add(100);
  _lut.clear();
}
////////////////////////////////////////////////////////////////////////////////
CameraDataLut::map_t::iterator CameraDataLut::begin(){
  return _lut.begin();
}
////////////////////////////////////////////////////////////////////////////////
CameraDataLut::map_t::const_iterator CameraDataLut::begin() const{
  return _lut.begin();
}
////////////////////////////////////////////////////////////////////////////////
CameraDataLut::map_t::iterator CameraDataLut::end(){
  return _lut.end();
}
////////////////////////////////////////////////////////////////////////////////
CameraDataLut::map_t::const_iterator CameraDataLut::end() const{
  return _lut.end();
}
////////////////////////////////////////////////////////////////////////////////
// CameraData
////////////////////////////////////////////////////////////////////////////////
CameraData::CameraData()
    : mEye(0.0f, 0.0f, 0.0f)
    , mTarget(0.0f, 0.0f, 1.0f)
    , mUp(0.0f, 1.0f, 0.0f)
    , _uiCamera(nullptr)
    , mAper(17.0f)
    , mHorizAper(0.0f)
    , mNear(100.0f)
    , mFar(750.0f){
}
////////////////////////////////////////////////////////////////////////////////
const fvec3& CameraData::GetEye() const {
  return mEye;
}
////////////////////////////////////////////////////////////////////////////////
const fvec3& CameraData::GetTarget() const {
  return mTarget;
}
////////////////////////////////////////////////////////////////////////////////
const fvec3& CameraData::GetUp() const {
  return mUp;
}
////////////////////////////////////////////////////////////////////////////////
float CameraData::GetNear() const {
  return mNear;
}
////////////////////////////////////////////////////////////////////////////////
float CameraData::GetFar() const {
  return mFar;
}
////////////////////////////////////////////////////////////////////////////////
float CameraData::GetAperature() const {
  return mAper;
}
////////////////////////////////////////////////////////////////////////////////
const fvec3& CameraData::xNormal() const {
  return _xnormal;
}
////////////////////////////////////////////////////////////////////////////////
const fvec3& CameraData::yNormal() const {
  return _ynormal;
}
////////////////////////////////////////////////////////////////////////////////
const fvec3& CameraData::zNormal() const {
  return _znormal;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::setXNormal(const fvec3& n) {
  _xnormal = n;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::setYNormal(const fvec3& n) {
  _ynormal = n;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::setZNormal(const fvec3& n) {
  _znormal = n;
}
////////////////////////////////////////////////////////////////////////////////
lev2::UiCamera* CameraData::getUiCamera() const {
  return _uiCamera;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::setUiCamera(lev2::UiCamera* pcam) {
  // printf( "CameraMatrices::setUiCamera() this<%p> pcam<%p>\n", this, pcam );
  _uiCamera = pcam;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::Persp(float fnear, float ffar, float faper) {
  mAper      = faper;
  mHorizAper = 0;
  mNear      = fnear;
  mFar       = ffar;
  _is_ortho = false;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::PerspH(float fnear, float ffar, float faperh) {
  mHorizAper = faperh;
  mNear      = fnear;
  mFar       = ffar;
  _is_ortho = false;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::Ortho(float left, float right, float top, float bottom,float fnear, float ffar) {
  mNear      = fnear;
  mFar       = ffar;
  _left = left;
  _right = right;
  _top = top;
  _bottom = bottom;
  _is_ortho = true;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::Lookat(const fvec3& eye, const fvec3& tgt, const fvec3& up) {
  mEye    = eye;
  mTarget = tgt;
  mUp     = up;
}
////////////////////////////////////////////////////////////////////////////////
void CameraData::fromPoseMatrix(const fmtx4& posemtx ){
  auto eye = posemtx.translation();
  auto tgt = eye + posemtx.column(2).xyz().normalized();
  auto up = posemtx.column(1).xyz().normalized();
  Lookat(eye,tgt,up);
}
////////////////////////////////////////////////////////////////////////////////
CameraMatrices CameraData::computeMatrices(float faspect) const {
  CameraMatrices rval;
  ///////////////////////////////
  // gameplay calculation
  ///////////////////////////////
  float faper = mAper;
  // float faspect = mfAspect;
  float fnear = mNear;
  float ffar  = mFar;
  ///////////////////////////////////////////////////
  if (faper < 0.2f)
    faper = 0.2f;
  if (fnear < 0.01f)
    fnear = 0.01f;
  if (ffar < 0.5f)
    ffar = 0.5f;
  if(fnear>ffar){
    ffar = fnear + 1;
  }
  ///////////////////////////////////////////////////
  fvec3 target = mTarget;
  ///////////////////////////////////////////////////
  float fmag2 = (target - mEye).magnitudeSquared();
  if (fmag2 < 0.01f) {
    target = mEye + fvec3::Blue();
  }
  ///////////////////////////////////////////////////
  if(_is_ortho){
    rval._pmatrix.ortho(_left,_right,_top,_bottom, fnear, ffar);
  }
  else{
    rval._pmatrix.perspective(faper, faspect, fnear, ffar);
  }
  rval._ipmatrix.inverseOf(rval._pmatrix);
  ///////////////////////////////////////////////////
  rval._vmatrix.lookAt(mEye, mTarget, mUp);
  rval._ivmatrix.inverseOf(rval._vmatrix);
  //rval._vpmatrix = fmtx4::multiply_ltor(rval._vmatrix,rval._pmatrix);
  rval._vpmatrix = fmtx4::multiply_ltor(rval._vmatrix,rval._pmatrix);
  rval._ivpmatrix.inverseOf(rval._vpmatrix);
  rval._frustum.set(rval._ivpmatrix);
  ///////////////////////////////////////////////////
  rval._camdat = *this;

  //deco::prints(rval._vmatrix.dump4x3cn(), true);
  //deco::prints(rval._pmatrix.dump4x3cn(), true);
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
template class ork::orklut<ork::PoolString, const ork::lev2::CameraData*>;
