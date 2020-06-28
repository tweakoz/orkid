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

  void deserializeItem() override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;
  void deserializeSharedObject(object_ptr_t&) override;
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
