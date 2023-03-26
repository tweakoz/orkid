////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#ifndef _ORK_STREAM_FILEINPUTSTREAM_H_
#define _ORK_STREAM_FILEINPUTSTREAM_H_

#include <ork/stream/IInputStream.h>
#include <ork/file/file.h>

namespace ork { namespace stream {

class FileInputStream : public IInputStream
{
public:
    FileInputStream(const char *filename);
    /*virtual*/ ~FileInputStream();
    /*virtual*/ size_t Read(unsigned char *buffer, size_type bufmax);
	bool IsOpen() const { return mFile.IsOpen(); }

    using IInputStream::size_type;
	using IInputStream::kEOF;
private:
    ork::File mFile;
};

} }

#endif
