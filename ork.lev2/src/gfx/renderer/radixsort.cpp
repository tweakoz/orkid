///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Contains source code from the article "Radix Sort Revisited".
 *	\file		IceRevisitedRadix.cpp
 *	\author		Pierre Terdiman
 *	\date		April, 4, 2000
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Revisited Radix Sort.
 *	This is my new radix routine:
 *  - it uses indices and doesn't recopy the values anymore, hence wasting less ram
 *  - it creates all the histograms in one run instead of four
 *  - it sorts words faster than dwords and bytes faster than words
 *  - it correctly sorts negative floating-point values by patching the offsets
 *  - it automatically takes advantage of temporal coherence
 *  - multiple keys support is a side effect of temporal coherence
 *  - it may be worth recoding in asm... (mainly to use FCOMI, FCMOV, etc) [it's probably memory-bound anyway]
 *
 *	History:
 *	- 08.15.98: very first version
 *	- 04.04.00: recoded for the radix article
 *	- 12.xx.00: code lifting
 *	- 09.18.01: faster CHECK_PASS_VALIDITY thanks to Mark D. Shattuck (who provided other tips, not included here)
 *	- 10.11.01: added local ram support
 *	- 01.20.02: bugfix! In very particular cases the last pass was skipped in the float code-path, leading to incorrect sorting......
 *
 *	\class		RadixSort
 *	\author		Pierre Terdiman
 *	\version	1.3
 *	\date		August, 15, 1998
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*
To do:
	- add an offset parameter between two input values (avoid some data recopy sometimes)
	- unroll ? asm ?
*/

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Precompiled Header

#include <ork/pch.h>
#include <ork/gfx/radixsort.h>

namespace ork { 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void RadixZeroMem( void* addr, U32 size)
{
	memset(addr, 0, size);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort::RadixSort() : mIndices(NULL), mIndices2(NULL), mCurrentSize(0), mPreviousSize(0), mTotalCalls(0), mNbHits(0)
{
#ifndef RADIX_LOCAL_RAM
	// Allocate input-independent ram
	mHistogram		= new U32[256*4];
	mOffset			= new U32[256];
#endif
	// Initialize indices
	ResetIndices();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Destructor.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort::~RadixSort()
{
	// Release everything
#ifndef RADIX_LOCAL_RAM
	delete[] mOffset;
	delete[] mHistogram;
#endif
	//delete[] mIndices2;
	//delete[] mIndices;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Resizes the inner lists.
 *	\param		nb				[in] new size (number of dwords)
 *	\return		true if success
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool RadixSort::Resize(U32 nb)
{
	// Free previously used ram
	delete[](mIndices2);
	delete[](mIndices);

	// Get some fresh one
	mIndices		= new U32[nb];	OrkAssertI( mIndices != 0, "radix mem prob" );
	mIndices2		= new U32[nb];	OrkAssertI( mIndices2 != 0, "radix mem prob" );
	mCurrentSize	= nb;

	// Initialize indices so that the input buffer is read in sequential order
	ResetIndices();

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RadixSort::CHECK_RESIZE( U32 n )
{
	if( n > mCurrentSize )
	{
		mIndices		= new U32[n];
		OrkAssertI( mIndices != 0, "radix mem prob" );
		mIndices2		= new U32[n];
		OrkAssertI( mIndices2 != 0, "radix mem prob" );
		mCurrentSize	= n;
	}

	// Initialize indices so that the input buffer is read in sequential order
	ResetIndices();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CHECK_PASS_VALIDITY(pass)																\
	/* Shortcut to current counters */															\
	U32* CurCount = &mHistogram[pass<<8];													\
																								\
	/* Reset flag. The sorting pass is supposed to be performed. (default) */					\
	bool PerformPass = true;																	\
																								\
	/* Check pass validity */																	\
																								\
	/* If all values have the same byte, sorting is useless. */									\
	/* It may happen when sorting bytes or words instead of dwords. */							\
	/* This routine actually sorts words faster than dwords, and bytes */						\
	/* faster than words. Standard running time (O(4*n))is reduced to O(2*n) */					\
	/* for words and O(n) for bytes. Running time for floats depends on actual values... */		\
																								\
	/* Get first byte */																		\
	U8 UniqueVal = *(((U8*)input)+pass);													\
																								\
	/* Check that byte's counter */																\
	if(CurCount[UniqueVal]==nb)	PerformPass=false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Main sort routine.
 *	This one is for integer values. After the call, mIndices contains a list of indices in sorted order, i.e. in the order you may process your data.
 *	\param		input			[in] a list of integer values to sort
 *	\param		nb				[in] number of values to sort
 *	\param		signedvalues	[in] true to handle negative values, false if you know your input buffer only contains positive values
 *	\return		Self-Reference
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort& RadixSort::Sort(const U32* input, U32 nb, bool signedvalues)
{
	// Checkings
	if(!input || !nb)	return *this;

	// Stats
	mTotalCalls++;

	// Resize lists if needed
	CHECK_RESIZE(nb);
	

#ifdef RADIX_LOCAL_RAM
	// Allocate histograms & offsets on the stack
	U32 mHistogram[256*4];
	U32 mOffset[256];
#endif

	// Create histograms (counters). Counters for all passes are created in one run.
	// Pros:	read input buffer once instead of four times
	// Cons:	mHistogram is 4Kb instead of 1Kb
	// We must take care of signed/unsigned values for temporal coherence.... I just
	// have 2 code paths even if just a single opcode changes. Self-modifying code, someone?
	if(!signedvalues)
	{
		bool AlreadySorted = CREATE_HISTOGRAMS<U32>(input,nb);

		if( AlreadySorted )
		{
			return *this;
		}
	}
	else
	{
		bool AlreadySorted = CREATE_HISTOGRAMS<S32>((S32*)input,nb);
		if( AlreadySorted )
		{
			return *this;
		}
	}

	// Compute #negative values involved if needed
	U32 NbNegativeValues = 0;
	if(signedvalues)
	{
		// An efficient way to compute the number of negatives values we'll have to deal with is simply to sum the 128
		// last values of the last histogram. Last histogram because that's the one for the Most Significant Byte,
		// responsible for the sign. 128 last values because the 128 first ones are related to positive numbers.
		U32* h3= &mHistogram[768];
		for(U32 i=128;i<256;i++)	NbNegativeValues += h3[i];	// 768 for last histogram, 128 for negative part
	}

	// Radix sort, j is the pass number (0=LSB, 3=MSB)
	for(U32 j=0;j<4;j++)
	{
		U32 k=j;

#if defined(WII) || defined(_XBOX)
		k = 3-j;
#endif
		CHECK_PASS_VALIDITY(k);

		// Sometimes the fourth (negative) pass is skipped because all numbers are negative and the MSB is 0xFF (for example). This is
		// not a problem, numbers are correctly sorted anyway.
		if(PerformPass)
		{
			// Should we care about negative values?
			if(k!=3 || !signedvalues)
			{
				// Here we deal with positive values only

				// Create offsets
				mOffset[0] = 0;
				for(U32 i=1;i<256;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];
			}
			else
			{
				// This is a special case to correctly handle negative integers. They're sorted in the right order but at the wrong place.

				// Create biased offsets, in order for negative numbers to be sorted as well
				mOffset[0] = NbNegativeValues;												// First positive number takes place after the negative ones
				for(U32 i=1;i<128;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];	// 1 to 128 for positive numbers

				// Fixing the wrong place for negative values
				mOffset[128] = 0;
				for( U32 i=129;i<256;i++)			mOffset[i] = mOffset[i-1] + CurCount[i-1];
			}

			// Perform Radix Sort
			U8* InputBytes	= (U8*)input;
			U32* Indices		= mIndices;
			U32* IndicesEnd	= &mIndices[nb];
			InputBytes += k;
			while(Indices!=IndicesEnd)
			{
				U32 id = *Indices++;
				mIndices2[mOffset[InputBytes[id<<2]]++] = id;
			}

			// Swap pointers for next pass. Valid indices - the most recent ones - are in mIndices after the swap.
			U32* Tmp	= mIndices;	mIndices = mIndices2; mIndices2 = Tmp;
		}
	}
	
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Main sort routine.
 *	This one is for floating-point values. After the call, mIndices contains a list of indices in sorted order, i.e. in the order you may process your data.
 *	\param		input			[in] a list of floating-point values to sort
 *	\param		nb				[in] number of values to sort
 *	\return		Self-Reference
 *	\warning	only sorts IEEE floating-point values
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort& RadixSort::Sort(const float* input2, U32 nb)
{
	// Checkings
	if(!input2 || !nb)	return *this;

	// Stats
	mTotalCalls++;

	U32* input = (U32*)input2;

	// Resize lists if needed
	CHECK_RESIZE(nb);

#ifdef RADIX_LOCAL_RAM
	// Allocate histograms & offsets on the stack
	U32 mHistogram[256*4];
	U32 mOffset[256];
#endif

	// Create histograms (counters). Counters for all passes are created in one run.
	// Pros:	read input buffer once instead of four times
	// Cons:	mHistogram is 4Kb instead of 1Kb
	// Floating-point values are always supposed to be signed values, so there's only one code path there.
	// Please note the floating point comparison needed for temporal coherence! Although the resulting asm code
	// is dreadful, this is surprisingly not such a performance hit - well, I suppose that's a big one on first
	// generation Pentiums....We can't make comparison on integer representations because, as Chris said, it just
	// wouldn't work with mixed positive/negative values....
	{ CREATE_HISTOGRAMS<float>(input2,nb); }

	// Compute #negative values involved if needed
	U32 NbNegativeValues = 0;
	// An efficient way to compute the number of negatives values we'll have to deal with is simply to sum the 128
	// last values of the last histogram. Last histogram because that's the one for the Most Significant Byte,
	// responsible for the sign. 128 last values because the 128 first ones are related to positive numbers.
	U32* h3= &mHistogram[768];
	for(U32 i=128;i<256;i++)	NbNegativeValues += h3[i];	// 768 for last histogram, 128 for negative part

	// Radix sort, j is the pass number (0=LSB, 3=MSB)
	for(U32 j=0;j<4;j++)
	{
		U32 k=j;

#if defined(WII) || defined(_XBOX)
		k = 3-j;
#endif

		// Should we care about negative values?
		if(k!=3)
		{
			// Here we deal with positive values only
			CHECK_PASS_VALIDITY(k);

			if(PerformPass)
			{
				// Create offsets
				mOffset[0] = 0;
				for(U32 i=1;i<256;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];

				// Perform Radix Sort
				U8* InputBytes	= (U8*)input;
				U32* Indices		= mIndices;
				U32* IndicesEnd	= &mIndices[nb];
				InputBytes += k;
				while(Indices!=IndicesEnd)
				{
					U32 id = *Indices++;
					mIndices2[mOffset[InputBytes[id<<2]]++] = id;
				}

				// Swap pointers for next pass. Valid indices - the most recent ones - are in mIndices after the swap.
				U32* Tmp	= mIndices;	mIndices = mIndices2; mIndices2 = Tmp;
			}
		}
		else
		{
			// This is a special case to correctly handle negative values
			CHECK_PASS_VALIDITY(k);

			if(PerformPass)
			{
				// Create biased offsets, in order for negative numbers to be sorted as well
				mOffset[0] = NbNegativeValues;												// First positive number takes place after the negative ones
				for(U32 i=1;i<128;i++)		mOffset[i] = mOffset[i-1] + CurCount[i-1];	// 1 to 128 for positive numbers

				// We must reverse the sorting order for negative numbers!
				mOffset[255] = 0;
				for(U32 i=0;i<127;i++)		mOffset[254-i] = mOffset[255-i] + CurCount[255-i];	// Fixing the wrong order for negative values
				for(U32 i=128;i<256;i++)	mOffset[i] += CurCount[i];							// Fixing the wrong place for negative values

				// Perform Radix Sort
				for(U32 i=0;i<nb;i++)
				{
					U32 Radix = input[mIndices[i]]>>24;								// Radix byte, same as above. AND is useless here (U32).
					// ### cmp to be killed. Not good. Later.
					if(Radix<128)		mIndices2[mOffset[Radix]++] = mIndices[i];		// Number is positive, same as above
					else				mIndices2[--mOffset[Radix]] = mIndices[i];		// Number is negative, flip the sorting order
				}
				// Swap pointers for next pass. Valid indices - the most recent ones - are in mIndices after the swap.
				U32* Tmp	= mIndices;	mIndices = mIndices2; mIndices2 = Tmp;
			}
			else
			{
				// The pass is useless, yet we still have to reverse the order of current list if all values are negative.
				if(UniqueVal>=128)
				{
					for(U32 i=0;i<nb;i++)	mIndices2[i] = mIndices[nb-i-1];

					// Swap pointers for next pass. Valid indices - the most recent ones - are in mIndices after the swap.
					U32* Tmp	= mIndices;	mIndices = mIndices2; mIndices2 = Tmp;
				}
			}
		}
	}
	
	return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Resets the inner indices. After the call, mIndices is reset.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RadixSort::ResetIndices()
{
	for(U32 i=0;i<mCurrentSize;i++)	mIndices[i] = i;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Gets the ram used.
 *	\return		memory used in bytes
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
U32 RadixSort::GetUsedRam() const
{
	U32 UsedRam = sizeof(RadixSort);
#ifndef RADIX_LOCAL_RAM
	UsedRam += 256*4*sizeof(U32);			// Histograms
	UsedRam += 256*sizeof(U32);				// Offsets
#endif
	UsedRam += 2*mCurrentSize*sizeof(U32);	// 2 lists of indices
	return UsedRam;
}

}
