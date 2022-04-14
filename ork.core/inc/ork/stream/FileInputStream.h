////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
