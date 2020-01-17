////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/serialize/LayerSerializer.h>

namespace ork { namespace reflect { namespace serialize {

class ShallowSerializer : public LayerSerializer
{
public:
	ShallowSerializer(ISerializer& serializer);

	/*virtual*/ bool Serialize(const rtti::ICastable *);
};

inline
ShallowSerializer::ShallowSerializer(ISerializer& serializer)
	: LayerSerializer(serializer)
{

}

inline
bool ShallowSerializer::Serialize(const rtti::ICastable* object)
{
	return LayerSerializer::Serialize(reinterpret_cast<long>(object));
}

} } }

