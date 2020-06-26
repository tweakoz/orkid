////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>

namespace ork { namespace reflect { namespace serialize {

class TextDeserializer : public IDeserializer {
public:
  TextDeserializer(stream::IInputStream& stream);

  void deserialize(bool&) override;
  void deserialize(char&) override;
  void deserialize(short&) override;
  void deserialize(int&) override;
  void deserialize(long&) override;
  void deserialize(float&) override;
  void deserialize(double&) override;

  void deserialize(MutableString&) override;
  void deserialize(ResizableString&) override;
  void deserializeData(uint8_t*, size_t) override;

  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;
  // void deserializeObject(rtti::ICastable*&) override;
  void deserializeSharedObject(object_ptr_t&) override;

  // bool ReferenceObject(rtti::ICastable*) override;
  void beginCommand(Command&) override;
  void endCommand(const Command&) override;

private:
  stream::InputStreamBuffer<128> mStream;

  void Advance();
  void EatSpace();
  void ReadWord(MutableString string);
  void ReadNumber(long& value);
  void ReadNumber(double& value);
  size_t Peek();
};

}}} // namespace ork::reflect::serialize
