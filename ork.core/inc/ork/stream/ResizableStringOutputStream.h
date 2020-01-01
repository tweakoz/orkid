///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#ifndef _ORK_STREAM_RESIZABLESTRINGOUTPUTSTREAM_H_
#define _ORK_STREAM_RESIZABLESTRINGOUTPUTSTREAM_H_

#include <ork/stream/IOutputStream.h>
#include <ork/kernel/string/ResizableString.h>

namespace ork { namespace stream {

class ResizableStringOutputStream : public IOutputStream
{
public:
    ResizableStringOutputStream(ResizableString &string);
    /*virtual*/ bool Write(const unsigned char *buffer, size_type bufsize);
private:
    ResizableString &mString;
};

} }

#endif
