////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


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
