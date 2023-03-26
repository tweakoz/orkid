////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#ifndef _ORK_STREAM_IOUTPUTSTREAM_H_
#define _ORK_STREAM_IOUTPUTSTREAM_H_

namespace ork { namespace stream {

class IOutputStream
{
public:
    typedef size_t size_type;
    virtual bool Write(const unsigned char *buffer, size_type bufmax) = 0;
    virtual ~IOutputStream();
};

} }

#endif
