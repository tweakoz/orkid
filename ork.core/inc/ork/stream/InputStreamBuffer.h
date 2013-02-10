///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////
#ifndef _ORK_STREAM_INPUTSTREAMBUFFER_H_
#define _ORK_STREAM_INPUTSTREAMBUFFER_H_

#include <ork/stream/IInputStream.h>

namespace ork { namespace stream {

template<size_t buffersize_>
class InputStreamBuffer : public IInputStream
{
public:
    InputStreamBuffer(IInputStream &stream);
    /*virtual*/ size_t Read(unsigned char dst[], size_t size);
    size_t Peek(unsigned char dst[], size_t size);
    /*virtual*/ void Discard(size_t size);

private:
    size_t PeekFromBuffer(unsigned char dst[], size_t size);
    void ReadFromBuffer(unsigned char dst[], size_t size);
    void ReadToBuffer(size_t size);

    IInputStream &mStream;
    size_t mAvailable;
    size_t mBufferStart;
    unsigned char mBuffer[buffersize_];
};

////////////////////////////////////////////////////////////////////

template<size_t buffersize_>
inline InputStreamBuffer<buffersize_>::InputStreamBuffer(IInputStream &stream)
	: mStream(stream)
	, mAvailable(0)
	, mBufferStart(0)
{}

template<size_t buffersize_>
inline size_t InputStreamBuffer<buffersize_>::Read(unsigned char dst[], size_t size)
{
	size_t read = 0;
	if(mAvailable > 0)
	{
		size_t piecesize = size < mAvailable ? size : mAvailable;

		ReadFromBuffer(dst, piecesize);
		read += piecesize;

		size -= piecesize;
		dst += piecesize;
	}

	if(size > 0)
	{
		size_t amount = mStream.Read(dst, size);
		if(amount != IInputStream::kEOF)
		   read += amount;
	}

	if(read == 0 && size > 0)
		return IInputStream::kEOF;

	return read;
}

template<size_t buffersize_>
inline size_t InputStreamBuffer<buffersize_>::Peek(unsigned char dst[], size_t size)
{
	if(mAvailable < size)
		ReadToBuffer(size - mAvailable);
	if(mAvailable == 0 && size > 0) return IInputStream::kEOF;
	else return PeekFromBuffer(dst, size);
}

template<size_t buffersize_>
inline void InputStreamBuffer<buffersize_>::Discard(size_t size)
{
	if(size < mAvailable)
	{
		mAvailable -= size;
		mBufferStart += size;
		while(mBufferStart >= buffersize_) mBufferStart -= buffersize_;
	}
	else
	{
		size -= mAvailable;
		mAvailable = 0;
		mBufferStart = 0;
		Read(NULL, size);
	//	mStream.Discard(size);
	/*	unsigned char c;
		while(size > 0 && Read(&c, 1) != IInputStream::kEOF)
		{
			size--;
		}*/
	}
}

template<size_t buffersize_>
inline size_t InputStreamBuffer<buffersize_>::PeekFromBuffer(unsigned char dst[], size_t size)
{
	size_t first_transfer = buffersize_ - mBufferStart;
	if(size > mAvailable) size = mAvailable;

	if(size <= first_transfer)
	{
	/*	if (size == 1)
			*dst = mBuffer[mBufferStart];
		else*/
			memcpy(dst, &mBuffer[mBufferStart], size);
	}
	else
	{
		size_t second_transfer = size - first_transfer;

		memcpy(dst, &mBuffer[mBufferStart], first_transfer);
		memcpy(dst + first_transfer, &mBuffer[0], second_transfer);
	}

	return size;
} 

template<size_t buffersize_>
inline void InputStreamBuffer<buffersize_>::ReadFromBuffer(unsigned char dst[], size_t size)
{
	if (dst != NULL)
		PeekFromBuffer(dst, size);
	Discard(size);
} 

template<size_t buffersize_>
inline void InputStreamBuffer<buffersize_>::ReadToBuffer(size_t size)
{
	size_t bufferend = (mBufferStart + mAvailable) % buffersize_;
	size_t first_transfer = buffersize_ - bufferend;

	if(first_transfer > size) first_transfer = size;

	size_t amount = mStream.Read(&mBuffer[bufferend], first_transfer);
	if(amount != IInputStream::kEOF)
	{
		mAvailable += amount;
		if(first_transfer < size)
		{
			size -= first_transfer;
			amount = mStream.Read(&mBuffer[0], size);
			if(amount != IInputStream::kEOF) mAvailable += amount;
		}
	}
}

} }

#endif