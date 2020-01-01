///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#ifndef _ORK_STREAM_STRINGINPUTSTREAM_H_
#define _ORK_STREAM_STRINGINPUTSTREAM_H_

#include <ork/stream/IInputStream.h>
#include <ork/kernel/string/PieceString.h>

namespace ork { namespace stream {

class StringInputStream : public IInputStream
{
public:
    StringInputStream(const PieceString &string);

    /*virtual*/ size_t Read(unsigned char *buffer, size_type bufmax);

	void SetStreamData(const PieceString &string);
private:
    static const size_t kEOF = IInputStream::kEOF;
    PieceString mString;
};

} }

#endif