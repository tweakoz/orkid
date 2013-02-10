///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	Contains source code from the article "Radix Sort Revisited".
//	author		Pierre Terdiman
//	date		April, 4, 2000
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __ICERADIXSORT_H__
#define __ICERADIXSORT_H__

namespace ork { 

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: Fix this
//#define RADIX_LOCAL_RAM

class RadixSort
{
	public:
	// Constructor/Destructor
	RadixSort();
	~RadixSort();
	// Sorting methods
	RadixSort&		Sort(const U32* input, U32 nb, bool signedvalues=true);
	RadixSort&		Sort(const float* input, U32 nb);

	//! Access to results. mIndices is a list of indices in sorted order, i.e. in the order you may further process your data
	inline	U32*			GetIndices()		const	{ return mIndices;		}

	//! mIndices2 gets trashed on calling the sort routine, but otherwise you can recycle it the way you want.
	inline	U32*			GetRecyclable()		const	{ return mIndices2;		}

	// Stats
	U32			GetUsedRam()		const;
	//! Returns the total number of calls to the radix sorter.
	inline	U32			GetNbTotalCalls()	const	{ return mTotalCalls;	}
	//! Returns the number of premature exits due to temporal coherence.
	inline	U32			GetNbHits()			const	{ return mNbHits;		}

	RadixSort(const RadixSort& object);
	RadixSort& operator=(const RadixSort& object);

private:

#ifndef RADIX_LOCAL_RAM
	U32*			mHistogram;					//!< Counters for each byte
	U32*			mOffset;					//!< Offsets (nearly a cumulative distribution function)
#endif
	U32			mCurrentSize;				//!< Current size of the indices list
	U32			mPreviousSize;				//!< Size involved in previous call
	U32*			mIndices;					//!< Two lists, swapped each pass
	U32*			mIndices2;
	// Stats
	U32			mTotalCalls;
	U32			mNbHits;
	// Internal methods
	bool			Resize(U32 nb);
	void			ResetIndices();



	void CHECK_RESIZE( U32 n );

    static void RadixZeroMem( void* addr, U32 size)
    {
		::memset(addr, 0, size);
    }

    template <typename type> bool CREATE_HISTOGRAMS( const type* buffer, const U32 nb )
    {
        /* Clear counters */
        RadixZeroMem((void*)this->mHistogram, 256*4*sizeof(U32));

        /* Prepare for temporal coherence */
        type PrevVal = type(buffer[mIndices[0]]);
        bool AlreadySorted = true;      /* Optimism... */
        U32* Indices = mIndices;

        /* Prepare to count */
        U8* p = (U8*)buffer;
        U8* pe = &p[nb*4];
        U32* h0= &mHistogram[0];                /* Histogram for first pass (LSB)       */
        U32* h1= &mHistogram[256];      /* Histogram for second pass            */
        U32* h2= &mHistogram[512];      /* Histogram for third pass                     */
        U32* h3= &mHistogram[768];      /* Histogram for last pass (MSB)        */

        while(p!=pe)
        {
            /* Read input buffer in previous sorted order */
            U32 Val = (U32)buffer[*Indices++];
            /* Check whether already sorted or not */
            if(type(Val)<PrevVal) { AlreadySorted = false; break; } /* Early out */
            /* Update for next iteration */
            PrevVal = type(Val);

            /* Create histograms */
            h0[*p++]++;     h1[*p++]++;     h2[*p++]++;     h3[*p++]++;
        }
        /* If all input values are already sorted, we just have to return and leave the */
        /* previous list unchanged. That way the routine may take advantage of temporal */
        /* coherence, for example when used to sort transparent faces.                                  */
        if(AlreadySorted)
        {
            mNbHits++;
            return true;
        }

        /* Else there has been an early out and we must finish computing the histograms */
        while(p!=pe)
        {
            /* Create histograms without the previous overhead */
            h0[*p++]++;     h1[*p++]++;     h2[*p++]++;     h3[*p++]++;
        }

        return AlreadySorted;

    }
};

} 


#endif // __ICERADIXSORT_H__
