///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <ork/stream/IOutputStream.h>
#include <ork/file/file.h>

namespace ork { namespace stream {

class FileOutputStream : public IOutputStream
{
public:
    FileOutputStream(const char *filename);
    ~FileOutputStream();
    bool Write(const unsigned char *buffer, size_type bufsize) final;
private:
    CFile mFile;
};

} }
