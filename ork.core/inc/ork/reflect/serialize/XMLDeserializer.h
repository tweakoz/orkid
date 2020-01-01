////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>

#include <ork/orkstl.h>

namespace ork { namespace reflect { namespace serialize {

class XMLDeserializer : public IDeserializer
{
public:
	XMLDeserializer(stream::IInputStream &stream);

    /*virtual*/ bool Deserialize(bool &);
	/*virtual*/ bool Deserialize(char &);
    /*virtual*/ bool Deserialize(short &);
    /*virtual*/ bool Deserialize(int &);
    /*virtual*/ bool Deserialize(long &);
    /*virtual*/ bool Deserialize(float &);
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
	bool EatBinaryData();
	bool DiscardData();
	bool DiscardCommandOrData(bool &error);

	int mLineNo;
	stream::InputStreamBuffer<1024*4> mStream;
	orkvector<rtti::ICastable *> mDeserializedObjects;

    void EatSpace();
    void Advance(int n = 1);

    int Peek();

	bool CheckLoose(const PieceString &s, size_t &matchlen);
	bool MatchLoose(const PieceString &s);

	bool Check(const PieceString &s);
    bool Match(const PieceString &s);

    bool ReadNumber(long &);
    bool ReadNumber(double &);

    size_t ReadWord(MutableString word);
	bool MatchEndTag(const ConstString &tagname);

    bool mbReadingAttributes;
	char mAttributeEndChar;
    const Command *mCurrentCommand;

	int FindObject(rtti::ICastable *object);

	////////////////////////////////////////////

	bool CheckExternalRead();
	bool BeginTag(const PieceString &tagname);
	bool EndTag(const PieceString &tagname);
	bool BeginAttribute(MutableString name);
	bool EndAttribute();
	bool ReadAttribute(MutableString name, MutableString value);
	
	template<typename StringType>
	bool ReadText(StringType &text);
	bool ReadBinary(unsigned char [], size_t);
	void ReadUntil(MutableString value, char terminator);
};

} } }
