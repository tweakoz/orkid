///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#ifndef _ORK_STREAM_STRINGOUTPUTSTREAM_H_
#define _ORK_STREAM_STRINGOUTPUTSTREAM_H_

#include <ork/stream/IOutputStream.h>
#include <ork/kernel/string/MutableString.h>

namespace ork { namespace stream {

class StringOutputStream : public IOutputStream
{
public:
    StringOutputStream(MutableString string);
    /*virtual*/ bool Write(const unsigned char *buffer, size_type bufsize);
private:
    MutableString mString;
};

} }

#endif
