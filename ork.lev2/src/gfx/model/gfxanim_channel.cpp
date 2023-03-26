////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>

using namespace std::string_literals;

ImplementReflectionX(ork::lev2::XgmAnimChannel, "XgmAnimChannel");
ImplementReflectionX(ork::lev2::XgmFloatAnimChannel, "XgmFloatAnimChannel");
ImplementReflectionX(ork::lev2::XgmVect3AnimChannel, "XgmVect3AnimChannel");
ImplementReflectionX(ork::lev2::XgmVect4AnimChannel, "XgmVect4AnimChannel");
//ImplementReflectionX(ork::lev2::XgmMatrixAnimChannel, "XgmMatrixAnimChannel");
ImplementReflectionX(ork::lev2::XgmDecompMatrixAnimChannel, "XgmDecompMatrixAnimChannel");

namespace ork::lev2 {

void XgmAnimChannel::describeX(class_t* clazz) {
}
void XgmFloatAnimChannel::describeX(class_t* clazz) {
}
void XgmVect3AnimChannel::describeX(class_t* clazz) {
}
void XgmVect4AnimChannel::describeX(class_t* clazz) {
}
//void XgmMatrixAnimChannel::describeX(class_t* clazz) {
//}
void XgmDecompMatrixAnimChannel::describeX(class_t* clazz) {
}

//XgmMatrixAnimChannel::~XgmMatrixAnimChannel() { // final
  //OrkAssert(false);
//}
XgmDecompMatrixAnimChannel::~XgmDecompMatrixAnimChannel() { // final
  OrkAssert(false);
}

///////////////////////////////////////////////////////////////////////////////

XgmAnimChannel::XgmAnimChannel(
    const std::string& ObjName,
    const std::string& ChanName,
    const std::string& UsageSemantic,
    EChannelType etype)
    : mChannelName(ChanName)
    , mObjectName(ObjName)
    , mUsageSemantic(UsageSemantic) 
    , miNumFrames(0)
    , meChannelType(etype) {
}

///////////////////////////////////////////////////////////////////////////////

XgmAnimChannel::XgmAnimChannel(EChannelType etype)
    : mChannelName()
    , mUsageSemantic() 
    , miNumFrames(0)
    , meChannelType(etype) {
}

///////////////////////////////////////////////////////////////////////////////

XgmFloatAnimChannel::XgmFloatAnimChannel()
    : XgmAnimChannel(EXGMAC_FLOAT) {
}

XgmFloatAnimChannel::XgmFloatAnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_FLOAT) {
}

///////////////////////////////////////////////////////////////////////////////

XgmVect3AnimChannel::XgmVect3AnimChannel()
    : XgmAnimChannel(EXGMAC_VECT3) {
}

XgmVect3AnimChannel::XgmVect3AnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_VECT3) {
}

///////////////////////////////////////////////////////////////////////////////

XgmVect4AnimChannel::XgmVect4AnimChannel()
    : XgmAnimChannel(EXGMAC_VECT4) {
}

XgmVect4AnimChannel::XgmVect4AnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_VECT4) {
}

///////////////////////////////////////////////////////////////////////////////
/*
XgmMatrixAnimChannel::XgmMatrixAnimChannel()
    : XgmAnimChannel(EXGMAC_MTX44) {
}

XgmMatrixAnimChannel::XgmMatrixAnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_MTX44) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmMatrixAnimChannel::setFrame(size_t i, const fmtx4& v) {
  _sampledFrames[i] = v;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmMatrixAnimChannel::GetFrame(int index) const {
  if(index>=_sampledFrames.size()){
    printf( "channel<%p:%s> getfr<%d> out of range %zd\n", this, mChannelName.c_str(), index, _sampledFrames.size() );
    OrkAssert(false);
    return fmtx4();
  }
  return _sampledFrames[index];
}

///////////////////////////////////////////////////////////////////////////////

size_t XgmMatrixAnimChannel::numFrames() const {
  return _sampledFrames.size();
}

///////////////////////////////////////////////////////////////////////////////

void XgmMatrixAnimChannel::reserveFrames(size_t icount) {
  _sampledFrames.resize(icount);
}*/

///////////////////////////////////////////////////////////////////////////////
// XgmDecompMatrixAnimChannel
///////////////////////////////////////////////////////////////////////////////

XgmDecompMatrixAnimChannel::XgmDecompMatrixAnimChannel()
    : XgmAnimChannel(EXGMAC_DCMTX) {
}

XgmDecompMatrixAnimChannel::XgmDecompMatrixAnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage)
    : XgmAnimChannel(ObjName, ChanName, Usage, EXGMAC_DCMTX) {
}

///////////////////////////////////////////////////////////////////////////////

void XgmDecompMatrixAnimChannel::setFrame(size_t i, const DecompMatrix& v) {
  _sampledFrames[i] = v;
}

///////////////////////////////////////////////////////////////////////////////

DecompMatrix XgmDecompMatrixAnimChannel::GetFrame(int index) const {
  if(index>=_sampledFrames.size()){
    printf( "channel<%p:%s> getfr<%d> out of range %zd\n", this, mChannelName.c_str(), index, _sampledFrames.size() );
    OrkAssert(false);
    return DecompMatrix();
  }
  return _sampledFrames[index];
}

///////////////////////////////////////////////////////////////////////////////

size_t XgmDecompMatrixAnimChannel::numFrames() const {
  return _sampledFrames.size();
}

///////////////////////////////////////////////////////////////////////////////

void XgmDecompMatrixAnimChannel::reserveFrames(size_t icount) {
  _sampledFrames.resize(icount);
}

} //namespace ork::lev2 {
