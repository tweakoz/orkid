#pragma once
////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IObjectPropertySharedObject.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class DirectObjectPropertySharedObject : public IObjectPropertySharedObject
{
    object_ptr_t Object::*mProperty;

public:
    DirectObjectPropertySharedObject(object_ptr_t Object::*);

    void get(object_ptr_t& value, const Object* obj) const;
    void set(object_ptr_t const& value, Object* obj) const;

    object_ptr_t Access(Object*) const            override;
    object_constptr_t Access(const Object*) const override;


    bool Deserialize(IDeserializer&, Object*) const   override;
    bool Serialize(ISerializer&, const Object*) const override;

};

} }
