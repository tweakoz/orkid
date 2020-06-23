////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork { namespace reflect {

BidirectionalSerializer::BidirectionalSerializer(IDeserializer& deserializer)
    : mDeserializer(&deserializer)
    , mSerializer(NULL)
    , mSuccess(true) {
}

BidirectionalSerializer::BidirectionalSerializer(ISerializer& serializer)
    : mDeserializer(NULL)
    , mSerializer(&serializer)
    , mSuccess(true) {
}

bool BidirectionalSerializer::Succeeded() const {
  return mSuccess;
}

bool BidirectionalSerializer::Serializing() const {
  return mSerializer != NULL;
}

ISerializer* BidirectionalSerializer::Serializer() const {
  return mSerializer;
}

IDeserializer* BidirectionalSerializer::Deserializer() const {
  return mDeserializer;
}

void BidirectionalSerializer::Fail() {
  mSuccess = false;
}

BidirectionalSerializer::operator bool() const {
  return Succeeded();
}

}} // namespace ork::reflect
