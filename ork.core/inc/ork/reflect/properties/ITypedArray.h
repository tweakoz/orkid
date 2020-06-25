////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <cstddef>
#include <ork/reflect/properties/IArray.h>

namespace ork { namespace reflect {

template<typename T>
class  ITypedArray : public IArray
{
public:
	ITypedArray() {}
    virtual void Get(T &value, const Object *obj, size_t index) const = 0;
    virtual void Set(const T &value, Object *obj, size_t index) const = 0;
private:
    /*virtual*/ bool DeserializeItem(IDeserializer &serializer, Object *obj, size_t index) const;
    /*virtual*/ bool SerializeItem(ISerializer &serializer, const Object *obj, size_t index) const;
};

} }

