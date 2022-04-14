////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


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
