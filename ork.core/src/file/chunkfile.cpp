////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#include <ork/pch.h>

#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/kernel/string/string.h>

namespace ork { namespace chunkfile {

///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddData( const void* ptr, size_t length )
{
  Write( (unsigned char*) ptr, length );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const bool& data )
{	bool temp = data;
    swapbytes_dynamic(temp);
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const unsigned char& data )
{	Write(&data, sizeof(data));
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const unsigned short& data )
{	unsigned short temp = data;
    swapbytes_dynamic(temp);
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const int& data )
{	int temp = data;
    swapbytes_dynamic(temp);
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const float& data )
{	float temp = data;
    swapbytes_dynamic(temp);
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const fmtx4& data )
{	fmtx4 temp = data;
    for( int i=0; i<16; i++ )
    {
        swapbytes_dynamic( temp.GetArray()[i] );
    }
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const fvec4& data )
{	fvec4 temp = data;
    for( int i=0; i<4; i++ )
    {
        swapbytes_dynamic( temp.GetArray()[i] );
    }
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const fvec3& data )
{	fvec3 temp = data;
    for( int i=0; i<3; i++ )
    {
        swapbytes_dynamic( temp.GetArray()[i] );
    }
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////
void OutputStream::AddItem( const fvec2& data )
{	fvec2 temp = data;
    for( int i=0; i<2; i++ )
    {
        swapbytes_dynamic( temp.GetArray()[i] );
    }
    Write( (unsigned char*) & temp, sizeof(temp) );
}
///////////////////////////////////////////////////////////////////////////////

bool OutputStream::Write(const unsigned char *buffer, size_type bufmax)  // virtual
{
  if( bufmax!=0 )
	 _data.insert( _data.end(), buffer, buffer+bufmax );
	return true;
}

///////////////////////////////////////////////////////////////////////////////

OutputStream::OutputStream()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

InputStream::InputStream( const void*pb, size_t ilength )
    : mpbase(pb)
    , midx(0)
    , milength(ilength)
{
}

const void * InputStream::GetCurrent()
{
    const char *pchbase = (const char*) mpbase;
    return (const void*) & pchbase[ midx ];
}

void* InputStream::GetDataAt( size_t idx )
{
    OrkAssert(idx<milength);
    const char *pchbase = (const char*) mpbase;
    return (void*) & pchbase[ idx ];
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Writer::Writer( const char* file_type )
{
	mFileType = GetStringIndex( file_type );
}

////////////////////////////////////////////////////////////////////////////////////

OutputStream* Writer::AddStream( std::string stream_name )
{
	OutputStream* nstream = new OutputStream;
	int ichunkid = string_block.AddString(stream_name.c_str()).Index();
	mOutputStreams.insert( std::make_pair(ichunkid, nstream) );
	return nstream;
}

////////////////////////////////////////////////////////////////////////////////////

int Writer::GetStringIndex( const char* pstr )
{
	return string_block.AddString(pstr).Index();
}

////////////////////////////////////////////////////////////////////////////////////

void Writer::WriteToFile( const file::Path& outpath )
{
	ork::File outputfile( outpath, ork::EFM_WRITE );

	Char4 chunk_magic("chkf");
	//swapbytes_dynamic( chunk_magic );

	////////////////////////
	outputfile.Write( & chunk_magic, sizeof(chunk_magic) );
	////////////////////////

	OutputStream	StringBlockStream;
	StringBlockStream.Write( (const unsigned char*) string_block.data(), string_block.size() );
	int istringblksize = StringBlockStream.GetSize();

	////////////////////////
	swapbytes_dynamic( istringblksize );
	outputfile.Write( & istringblksize, sizeof(istringblksize) );
	outputfile.Write( StringBlockStream.GetData(), StringBlockStream.GetSize() );
	////////////////////////
	swapbytes_dynamic( mFileType );
	outputfile.Write( & mFileType, sizeof(mFileType) );
	////////////////////////
	int inumchunks = (int) mOutputStreams.size();
	swapbytes_dynamic( inumchunks );
	outputfile.Write( & inumchunks, sizeof(inumchunks) );
	////////////////////////

	int ioffset = 0;
	for( orkmap<int,OutputStream*>::const_iterator it=mOutputStreams.begin(); it!=mOutputStreams.end(); it++ )
	{
		int ichunkid = it->first;
		OutputStream* stream = it->second;
		int ichunklen = stream->GetSize();
		////////////////////////
		swapbytes_dynamic( ichunkid );
		swapbytes_dynamic( ioffset );
		swapbytes_dynamic( ichunklen );
		outputfile.Write( & ichunkid, sizeof(ichunkid) );
		outputfile.Write( & ioffset, sizeof(ioffset) );
		outputfile.Write( & ichunklen, sizeof(ichunklen) );
		////////////////////////

		ioffset += ichunklen;
	}

	////////////////////////
	for( orkmap<int,OutputStream*>::const_iterator it=mOutputStreams.begin(); it!=mOutputStreams.end(); it++ )
	{
		int ichunkid = it->first;
		OutputStream* stream = it->second;
		int ichunklen = stream->GetSize();
		if( ichunklen && stream->GetData() )
		{
			outputfile.Write( stream->GetData(), ichunklen );
		}
	}

}

} } // namespace ork::chunkfile
