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

  bool Deserialize(bool&) override;
  bool Deserialize(char&) override;
  bool Deserialize(short&) override;
  bool Deserialize(int&) override;
  bool Deserialize(long&) override;
  bool Deserialize(float&) override;
  bool Deserialize(double&) override;

  bool Deserialize(MutableString&) override;
  bool Deserialize(ResizableString&) override;
  bool DeserializeData(unsigned char*, size_t) override;

  bool Deserialize(const IProperty*) override;
  bool deserializeObjectProperty(const I*, Object*) override;
  bool deserializeObject(rtti::ICastable*&) override;
  bool deserializeSharedObject(rtti::castable_ptr_t&) override;

  bool ReferenceObject(rtti::ICastable*) override;
  bool BeginCommand(Command&) override;
  bool EndCommand(const Command&) override;

private:
  stream::InputStreamBuffer<128> mStream;

  void Advance();
  void EatSpace();
  size_t ReadWord(MutableString string);
  bool ReadNumber(long& value);
  bool ReadNumber(double& value);
  size_t Peek();
};

}}} // namespace ork::reflect::serialize
