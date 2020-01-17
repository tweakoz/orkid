////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/ISerializer.h>
#include <ork/orkstl.h>
#include <ork/rtti/Category.h>

namespace ork { namespace stream { class IOutputStream; } }

namespace ork { namespace reflect { namespace serialize {

class XMLSerializer : public ISerializer
{
public:
	XMLSerializer(stream::IOutputStream &stream);

    /*virtual*/ bool Serialize(const bool            &);
    /*virtual*/ bool Serialize(const char            &);
    /*virtual*/ bool Serialize(const short           &);
    /*virtual*/ bool Serialize(const int             &);
	/*virtual*/ bool Serialize(const unsigned int    &);
    /*virtual*/ bool Serialize(const long            &);
    /*virtual*/ bool Serialize(const float           &);
    /*virtual*/ bool Serialize(const double          &);
	/*virtual*/ bool Serialize(const rtti::ICastable *);
    /*virtual*/ bool Serialize(const PieceString     &);
	/*virtual*/ void Hint(const PieceString &);
	/*virtual*/ void Hint(const PieceString &, intptr_t ival) {}

    /*virtual*/ bool SerializeData(unsigned char *, size_t size);

	/*virtual*/ bool Serialize(const IProperty *);
	/*virtual*/ bool Serialize(const IObjectProperty *, const Object *);

	/*virtual*/ bool ReferenceObject(const rtti::ICastable *);
    /*virtual*/ bool BeginCommand(const Command &);
    /*virtual*/ bool EndCommand(const Command &);

	bool Serialize(const rtti::Category *category, const rtti::ICastable *object);

private:
    stream::IOutputStream &mStream;
	orkvector<const rtti::ICastable *> mSerializedObjects;
    int mIndent;
    bool mbWritingAttributes;
    bool mbNeedSpace;
    bool mbNeedLine;
    const Command *mCurrentCommand;
    void Spaced();
	void Lined();
	void Unspaced();
    
	bool Write(char *text, size_t size);
	bool WriteText(const char *format, ...);
    
	bool FlushHeader();

	bool StartObject(PieceString name);
    bool EndObject();
	int FindObject(const rtti::ICastable *);
};

} } }
