////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
