////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
