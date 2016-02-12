///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

namespace ork { namespace stream {

class IOutputStream
{
public:
    typedef size_t size_type;
    virtual bool Write(const unsigned char *buffer, size_type bufmax) = 0;
    virtual ~IOutputStream();
};

} }
