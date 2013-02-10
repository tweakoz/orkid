////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/audiobank.h>
#include "audiodevice_xa2.h"
#if defined(USE_XWMA)
#include <ork/file/file.h>
#include <ork/file/riff.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/mutex.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/array.hpp>
#include <ork/util/endian.h>
#include <xmp.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace lev2 {

/////////////////////////////////////////////////////

XWMAStream::XWMAStream()
	: mpFile( 0 )
{
	memset( (void*) & mFormat, 0, sizeof(mFormat) );
}

/////////////////////////////////////////////////////

XWMAStream::~XWMAStream()
{
    Close();
	if( mpFile )
		delete mpFile;
}

/////////////////////////////////////////////////////

bool XWMAStream::Open( const ork::file::Path& pth )
{
	mpFile = new ork::RiffFile2;

	mpFile->Open( pth );

	int inumchunks = mpFile->Chunks.size();

	for( int ic=0; ic<inumchunks; ic++ )
	{
		const ork::RiffChunk2* chunk = mpFile->Chunks[ic];
		const char* pbase = chunk->data;
		int ilen = chunk->chunklength;

		struct Reader
		{
			const char* pbase;

			void doit( void* pdest, int isize, int& ioffset )
			{
				memcpy( pdest, (const void*) (pbase+ioffset), isize );
				ioffset += isize;
			}
		};

		Reader myreader;
		myreader.pbase = pbase;

		if( 0 == strcmp(chunk->chunkname,"fmt ") )
		{
			int ipos = 0;
	
			myreader.doit( (void*)& mFormat.wfx, sizeof(mFormat.wfx), ipos );
			
			swapbytes_from_little( mFormat.wfx.cbSize );
			swapbytes_from_little( mFormat.wfx.nAvgBytesPerSec );
			swapbytes_from_little( mFormat.wfx.nBlockAlign );
			swapbytes_from_little( mFormat.wfx.nChannels );
			swapbytes_from_little( mFormat.wfx.nSamplesPerSec );
			swapbytes_from_little( mFormat.wfx.wBitsPerSample );
			swapbytes_from_little( mFormat.wfx.wFormatTag );

			if(ilen > sizeof(mFormat.wfx))  
			{  
				myreader.doit( (void*)& mFormat.NumStreams, sizeof(mFormat.NumStreams), ipos );
				myreader.doit( (void*)& mFormat.ChannelMask, sizeof(mFormat.ChannelMask), ipos );
				myreader.doit( (void*)& mFormat.SamplesEncoded, sizeof(mFormat.SamplesEncoded), ipos );
				myreader.doit( (void*)& mFormat.BytesPerBlock, sizeof(mFormat.BytesPerBlock), ipos );
				myreader.doit( (void*)& mFormat.EncoderVersion, sizeof(mFormat.EncoderVersion), ipos );
				myreader.doit( (void*)& mFormat.BlockCount, sizeof(mFormat.BlockCount), ipos );

				swapbytes_from_little( mFormat.NumStreams );
				swapbytes_from_little( mFormat.ChannelMask );
				swapbytes_from_little( mFormat.SamplesEncoded );
				swapbytes_from_little( mFormat.BytesPerBlock );
				swapbytes_from_little( mFormat.EncoderVersion );
				swapbytes_from_little( mFormat.BlockCount );
			}  

		}
		else if( 0 == strcmp(chunk->chunkname,"data") )
		{
			mpAudioData = pbase;
			miAudioLength = ilen;
		}
		else if( 0 == strcmp(chunk->chunkname,"dpds") )
		{
			mpDPDS = pbase;
			miDPDSLength = ilen;
			
			mBufferWma.PacketCount = ilen/4;
			mBufferWma.pDecodedPacketCumulativeBytes = const_cast<UINT32*>((UINT32*)mpDPDS);

			for( UINT32 i=0; i<mBufferWma.PacketCount; i++ )
			{
				swapbytes_from_little( mBufferWma.pDecodedPacketCumulativeBytes[i] );
			}
		}
	}

	mAudioBuffer.Flags = XAUDIO2_END_OF_STREAM;
	mAudioBuffer.AudioBytes = GetAudioDataLen();
	mAudioBuffer.pAudioData = (BYTE*) GetAudioData();
	mAudioBuffer.PlayBegin = 0;
	mAudioBuffer.PlayLength = 0;
	mAudioBuffer.pContext = (void*) this;
	///////////////////////////////////////
	mAudioBuffer.LoopBegin  = XAUDIO2_NO_LOOP_REGION;
	mAudioBuffer.LoopLength  = 0;
	mAudioBuffer.LoopCount = 0; 

	return true;
}

/////////////////////////////////////////////////////

bool XWMAStream::Close()
{
    return true;
}

/////////////////////////////////////////////////////
}}
#endif
