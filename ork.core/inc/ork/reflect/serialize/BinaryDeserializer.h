////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>
#include <ork/kernel/string/StringPool.h>


namespace ork { namespace reflect { namespace serialize {

class BinaryDeserializer : public IDeserializer
{
public:
	BinaryDeserializer(stream::IInputStream &stream);
	~BinaryDeserializer();

    /*virtual*/ bool Deserialize(bool   &);
	/*virtual*/ bool Deserialize(char   &);
    /*virtual*/ bool Deserialize(short  &);
    /*virtual*/ bool Deserialize(int    &);
    /*virtual*/ bool Deserialize(long   &);
    /*virtual*/ bool Deserialize(float  &);
    /*virtual*/ bool Deserialize(double &);
	/*virtual*/ bool Deserialize(rtti::ICastable *&);

	/*virtual*/ bool Deserialize(const IProperty *);
	/*virtual*/ bool Deserialize(const IObjectProperty *, Object *);

    /*virtual*/ bool Deserialize(MutableString &);
    /*virtual*/ bool Deserialize(ResizableString &);
    /*virtual*/ bool DeserializeData(unsigned char *, size_t);

    /*virtual*/ bool ReferenceObject(rtti::ICastable *);
    /*virtual*/ bool BeginCommand(Command &);
    /*virtual*/ bool EndCommand(const Command &);
private:
	int FindObject(rtti::ICastable *object);

	template<typename T>
	bool Read(T &);

	char Peek();
	bool Match(char c);

	stream::InputStreamBuffer<1> mStream;
	orkvector<rtti::ICastable *> mDeserializedObjects;
	const Command *mCurrentCommand;
	StringPool mStringPool;
};

} } }

