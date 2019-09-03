///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#ifndef _ORK_STREAM_FILEOUTPUTSTREAM_H_
#define _ORK_STREAM_FILEOUTPUTSTREAM_H_

#include <ork/stream/IOutputStream.h>
#include <ork/file/file.h>

namespace ork { namespace stream {

class FileOutputStream : public IOutputStream
{
public:
    FileOutputStream(const char *filename);
    ~FileOutputStream();
    /*virtual*/ bool Write(const unsigned char *buffer, size_type bufsize);
private:
    File mFile;
};

} }

#endif
