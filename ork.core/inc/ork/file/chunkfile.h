////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/file.h>
#include <ork/util/endian.h>
#include <ork/stream/IOutputStream.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector2.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/string/PoolString.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

namespace chunkfile {

class OutputStream : public ork::stream::IOutputStream
{
public:

	bool Write(const unsigned char *buffer, size_type bufmax);  // virtual

	OutputStream();

	void reserve(size_t len) { _data.reserve(len); }

	/////////////////////////////////////////////
	template <typename T> void AddItem( const T& data );
	void AddItem( const bool& data );
	void AddItem( const unsigned char& data );
	void AddItem( const unsigned short& data );
	void AddItem( const int& data );
	void AddItem( const float& data );
	void AddItem( const fmtx4& data );
	void AddItem( const fvec4& data );
	void AddItem( const fvec3& data );
	void AddItem( const fvec2& data );
	void AddData( const void* ptr, size_t length );
	/////////////////////////////////////////////

	size_t GetSize() const { return _data.size(); }
	const void* GetData() const { return GetSize() ? & _data[0] : 0; }

	/////////////////////////////////////////////

private:
	std::vector<uint8_t>	_data;
};

///////////////////////////////////////////////////////////////////////////////

class Writer
{
public:
	////////////////////////////////////////////////////////////////////////////////////

	Writer( const char* file_type );
	OutputStream* AddStream( std::string stream_name );
	int GetStringIndex( const char* pstr );
	void WriteToFile( const file::Path& outpath );

	////////////////////////////////////////////////////////////////////////////////////

private:

	StringBlock			string_block;
	int					mFileType;

	orkmap<int,OutputStream*> mOutputStreams;
};

struct InputStream
{
	InputStream( const void*pb=0, size_t ilength=0 );
	template< typename T > void GetItem( T &item );
	template< typename T > void RefItem( T* &item );

	const void * GetCurrent();
	void * GetDataAt( size_t idx );
	size_t GetLength() const { return milength; }
	void advance(size_t l) { midx+=l;}
	const void*	mpbase;
	size_t			midx;
	size_t			milength;

};

///////////////////////////////////////////////////////////////////////////////

template <typename Allocator> class Reader
{
	static const int kmaxstreams = 16;
	InputStream mStreamBank[kmaxstreams];
	typedef ork::fixedlut<ork::PoolString,InputStream*,kmaxstreams> StreamLut;

	Allocator mAllocator;

public:

	Reader( const file::Path& inpath, const char* ptype );
	~Reader();

	InputStream* GetStream( const char* streamname );
	const char* GetString( int index );

	bool IsOk() const { return mbOk; }

private:

	int	mistrtablen;
	const char* mpstrtab;
	bool mbOk;

	StreamLut mInputStreams;
};

///////////////////////////////////////////////////////////////////////////////
} } // namespace ork/chunkfile
///////////////////////////////////////////////////////////////////////////////
