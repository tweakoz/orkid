////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/ISerializer.h> // for base

namespace ork { namespace rtti { class Category; } }

namespace ork { namespace reflect { namespace serialize {

class NullSerializer : public ISerializer
{
public:
	/*virtual*/ bool Serialize(const bool   &);
    /*virtual*/ bool Serialize(const char   &);
    /*virtual*/ bool Serialize(const short  &);
    /*virtual*/ bool Serialize(const int    &);
    /*virtual*/ bool Serialize(const long   &);
    /*virtual*/ bool Serialize(const float  &);
    /*virtual*/ bool Serialize(const double &);
    /*virtual*/ bool Serialize(const rtti::ICastable *);
    /*virtual*/ bool Serialize(const PieceString &);

	/*virtual*/ void Hint(const PieceString &);
    /*virtual*/ void Hint(const PieceString &, intptr_t ival);

    /*virtual*/ bool SerializeData(unsigned char *, size_t);

	/*virtual*/ bool Serialize(const IProperty *prop);
	/*virtual*/ bool Serialize(const IObjectProperty *prop, const Object *object);
	/*virtual*/ bool Serialize(const rtti::Category *cat, const rtti::ICastable *object);

	/*virtual*/ bool ReferenceObject(const rtti::ICastable *);

    /*virtual*/ bool BeginCommand(const Command &);
	/*virtual*/ bool EndCommand(const Command &);
};

} } }

