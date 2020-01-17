////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/serialize/LayerDeserializer.h>

namespace ork { namespace reflect { namespace serialize {

class ShallowDeserializer : public LayerDeserializer
{
public:
	ShallowDeserializer(IDeserializer& deserializer);

	/*virtual*/ bool Deserialize(rtti::ICastable*&);
};

inline
ShallowDeserializer::ShallowDeserializer(IDeserializer& deserializer)
	: LayerDeserializer(deserializer)
{

}

inline
bool ShallowDeserializer::Deserialize(rtti::ICastable*& object)
{
	long deserialized;
	bool result = LayerDeserializer::Deserialize(deserialized);
	object = reinterpret_cast<rtti::ICastable*>(deserialized);
	return result;
}

} } }

