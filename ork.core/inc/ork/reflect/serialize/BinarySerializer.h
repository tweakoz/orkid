////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/kernel/orkvector.h>
#include <ork/kernel/string/StringPool.h>

namespace ork { namespace stream { class IOutputStream; } }

namespace ork { namespace reflect { namespace serialize {

class BinarySerializer : public ISerializer
{
public:
	BinarySerializer(stream::IOutputStream &stream);
	~BinarySerializer();

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
	/*virtual*/ void Hint(const PieceString &,intptr_t ival);

    /*virtual*/ bool SerializeData(unsigned char *, size_t size);

	/*virtual*/ bool Serialize(const IProperty *);
	/*virtual*/ bool Serialize(const IObjectProperty *, const Object *);

	/*virtual*/ bool ReferenceObject(const rtti::ICastable *);
    /*virtual*/ bool BeginCommand(const Command &);
    /*virtual*/ bool EndCommand(const Command &);
private:
	int FindObject(const rtti::ICastable *object);

	bool WriteHeader(char type, PieceString text);
	bool WriteFooter(char type);
	template<typename T>
	bool Write(const T &datum);

    stream::IOutputStream &mStream;
	orkvector<const rtti::ICastable *> mSerializedObjects;
	StringPool mStringPool;
	const Command *mCurrentCommand;
};

} } }

