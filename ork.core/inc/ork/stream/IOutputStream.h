///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
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
