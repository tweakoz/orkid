////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
