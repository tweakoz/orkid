////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/BidirectionalSerializer.h>

namespace ork::reflect::serdes {

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

} // namespace ork::reflect::serdes
