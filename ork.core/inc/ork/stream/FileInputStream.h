///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
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
    ork::CFile mFile;
};

} }

#endif
