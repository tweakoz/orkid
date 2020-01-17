///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	contains source code from the article "Radix Sort Revisited".
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
 *	- 01.20.02: bugfix! In very particular cases the last pass was skipped in the float code-path, leading to incorrect
 *sorting......
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

#include <ork/gfx/radixsort.h>
#include <ork/pch.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort::RadixSort()
    : _indices(nullptr)
    , _indices2(nullptr)
    , _currentSize(0)
    , _previousSize(0)
    , _totalCalls(0)
    , _numBhits(0) {
  _histogram = new uint32_t[HISTOSIZE];
  _offset    = new uint32_t[HISTOSIZE];
  resetIndices();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Destructor.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort::~RadixSort() {
  delete[] _offset;
  delete[] _histogram;
  if (_indices2)
    delete[] _indices2;
  if (_indices)
    delete[] _indices;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void RadixSort::CHECK_RESIZE(size_t n) {
  if (n > _currentSize) {
    if (_indices)
      delete[] _indices;
    if (_indices2)
      delete[] _indices2;
    _indices     = new uint32_t[n];
    _indices2    = new uint32_t[n];
    _currentSize = n;
  }

  // Initialize indices so that the input buffer is read in sequential order
  resetIndices();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define CHECK_PASS_VALIDITY(pass)                                                                                                  \
  /* Shortcut to current counters */                                                                                               \
  uint32_t* CurCount = &_histogram[pass << 8];                                                                                     \
                                                                                                                                   \
  /* Reset flag. The sorting pass is supposed to be performed. (default) */                                                        \
  bool PerformPass = true;                                                                                                         \
                                                                                                                                   \
  /* Check pass validity */                                                                                                        \
                                                                                                                                   \
  /* If all values have the same byte, sorting is useless. */                                                                      \
  /* It may happen when sorting bytes or words instead of dwords. */                                                               \
  /* This routine actually sorts words faster than dwords, and bytes */                                                            \
  /* faster than words. Standard running time (O(4*n))is reduced to O(2*n) */                                                      \
  /* for words and O(n) for bytes. Running time for floats depends on actual values... */                                          \
                                                                                                                                   \
  /* Get first byte */                                                                                                             \
  uint8_t UniqueVal = *(((uint8_t*)input) + pass);                                                                                 \
                                                                                                                                   \
  /* Check that byte's counter */                                                                                                  \
  if (CurCount[UniqueVal] == nb)                                                                                                   \
    PerformPass = false;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Main sort routine.
 *	This one is for integer values. After the call, _indices contains a list of indices in sorted order, i.e. in the order you may
 *process your data. \param		input			[in] a list of integer values to sort
 *	\param		nb				[in] number of values to sort
 *	\param		signedvalues	[in] true to handle negative values, false if you know your input buffer only contains positive
 *values \return		Self-Reference
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort& RadixSort::Sort(const uint32_t* input, size_t nb, bool signedvalues) {
  // Checkings
  if (!input || !nb)
    return *this;

  // Stats
  _totalCalls++;

  // resize lists if needed
  CHECK_RESIZE(nb);

  // Create histograms (counters). Counters for all passes are created in one run.
  // Pros:	read input buffer once instead of four times
  // Cons:	_histogram is 4Kb instead of 1Kb
  // We must take care of signed/unsigned values for temporal coherence.... I just
  // have 2 code paths even if just a single opcode changes. Self-modifying code, someone?
  if (!signedvalues) {
    bool AlreadySorted = CREATE_HISTOGRAMS<uint32_t>(input, nb);

    if (AlreadySorted) {
      return *this;
    }
  } else {
    bool AlreadySorted = CREATE_HISTOGRAMS<S32>((S32*)input, nb);
    if (AlreadySorted) {
      return *this;
    }
  }

  // Compute #negative values involved if needed
  uint32_t NbNegativeValues = 0;
  if (signedvalues) {
    // An efficient way to compute the number of negatives values we'll have to deal with is simply to sum the 128
    // last values of the last histogram. Last histogram because that's the one for the Most Significant Byte,
    // responsible for the sign. 128 last values because the 128 first ones are related to positive numbers.
    uint32_t* h3 = &_histogram[768];
    for (uint32_t i = 128; i < 256; i++)
      NbNegativeValues += h3[i]; // 768 for last histogram, 128 for negative part
  }

  // Radix sort, j is the pass number (0=LSB, 3=MSB)
  for (uint32_t j = 0; j < 4; j++) {
    uint32_t k = j;

    CHECK_PASS_VALIDITY(k);

    // Sometimes the fourth (negative) pass is skipped because all numbers are negative and the MSB is 0xFF (for example). This is
    // not a problem, numbers are correctly sorted anyway.
    if (PerformPass) {
      // Should we care about negative values?
      if (k != 3 || !signedvalues) {
        // Here we deal with positive values only

        // Create offsets
        _offset[0] = 0;
        for (uint32_t i = 1; i < 256; i++)
          _offset[i] = _offset[i - 1] + CurCount[i - 1];
      } else {
        // This is a special case to correctly handle negative integers. They're sorted in the right order but at the wrong place.

        // Create biased offsets, in order for negative numbers to be sorted as well
        _offset[0] = NbNegativeValues; // First positive number takes place after the negative ones
        for (uint32_t i = 1; i < 128; i++)
          _offset[i] = _offset[i - 1] + CurCount[i - 1]; // 1 to 128 for positive numbers

        // Fixing the wrong place for negative values
        _offset[128] = 0;
        for (uint32_t i = 129; i < 256; i++)
          _offset[i] = _offset[i - 1] + CurCount[i - 1];
      }

      // Perform Radix Sort
      uint8_t* InputBytes  = (uint8_t*)input;
      uint32_t* Indices    = _indices;
      uint32_t* IndicesEnd = &_indices[nb];
      InputBytes += k;
      while (Indices != IndicesEnd) {
        uint32_t id                               = *Indices++;
        _indices2[_offset[InputBytes[id << 2]]++] = id;
      }

      // Swap pointers for next pass. Valid indices - the most recent ones - are in _indices after the swap.
      uint32_t* Tmp = _indices;
      _indices      = _indices2;
      _indices2     = Tmp;
    }
  }

  return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Main sort routine.
 *	This one is for floating-point values. After the call, _indices contains a list of indices in sorted order, i.e. in the order
 *you may process your data. \param		input			[in] a list of floating-point values to sort \param		nb				[in]
 *number of values to sort \return		Self-Reference \warning	only sorts IEEE floating-point values
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
RadixSort& RadixSort::Sort(const float* input2, size_t nb) {
  // Checkings
  if (!input2 || !nb)
    return *this;

  // Stats
  _totalCalls++;

  uint32_t* input = (uint32_t*)input2;

  // resize lists if needed
  CHECK_RESIZE(nb);

  // Create histograms (counters). Counters for all passes are created in one run.
  // Pros:	read input buffer once instead of four times
  // Cons:	_histogram is 4Kb instead of 1Kb
  // Floating-point values are always supposed to be signed values, so there's only one code path there.
  // Please note the floating point comparison needed for temporal coherence! Although the resulting asm code
  // is dreadful, this is surprisingly not such a performance hit - well, I suppose that's a big one on first
  // generation Pentiums....We can't make comparison on integer representations because, as Chris said, it just
  // wouldn't work with mixed positive/negative values....
  { CREATE_HISTOGRAMS<float>(input2, nb); }

  // Compute #negative values involved if needed
  uint32_t NbNegativeValues = 0;
  // An efficient way to compute the number of negatives values we'll have to deal with is simply to sum the 128
  // last values of the last histogram. Last histogram because that's the one for the Most Significant Byte,
  // responsible for the sign. 128 last values because the 128 first ones are related to positive numbers.
  uint32_t* h3 = &_histogram[768];
  for (uint32_t i = 128; i < 256; i++)
    NbNegativeValues += h3[i]; // 768 for last histogram, 128 for negative part

  // Radix sort, j is the pass number (0=LSB, 3=MSB)
  for (uint32_t j = 0; j < 4; j++) {
    uint32_t k = j;

    // Should we care about negative values?
    if (k != 3) {
      // Here we deal with positive values only
      CHECK_PASS_VALIDITY(k);

      if (PerformPass) {
        // Create offsets
        _offset[0] = 0;
        for (uint32_t i = 1; i < 256; i++)
          _offset[i] = _offset[i - 1] + CurCount[i - 1];

        // Perform Radix Sort
        uint8_t* InputBytes  = (uint8_t*)input;
        uint32_t* Indices    = _indices;
        uint32_t* IndicesEnd = &_indices[nb];
        InputBytes += k;
        while (Indices != IndicesEnd) {
          uint32_t id                               = *Indices++;
          _indices2[_offset[InputBytes[id << 2]]++] = id;
        }

        // Swap pointers for next pass. Valid indices - the most recent ones - are in _indices after the swap.
        uint32_t* Tmp = _indices;
        _indices      = _indices2;
        _indices2     = Tmp;
      }
    } else {
      // This is a special case to correctly handle negative values
      CHECK_PASS_VALIDITY(k);

      if (PerformPass) {
        // Create biased offsets, in order for negative numbers to be sorted as well
        _offset[0] = NbNegativeValues; // First positive number takes place after the negative ones
        for (uint32_t i = 1; i < 128; i++)
          _offset[i] = _offset[i - 1] + CurCount[i - 1]; // 1 to 128 for positive numbers

        // We must reverse the sorting order for negative numbers!
        _offset[255] = 0;
        for (uint32_t i = 0; i < 127; i++)
          _offset[254 - i] = _offset[255 - i] + CurCount[255 - i]; // Fixing the wrong order for negative values
        for (uint32_t i = 128; i < 256; i++)
          _offset[i] += CurCount[i]; // Fixing the wrong place for negative values

        // Perform Radix Sort
        for (uint32_t i = 0; i < nb; i++) {
          uint32_t Radix = input[_indices[i]] >> 24; // Radix byte, same as above. AND is useless here (uint32_t).
          // ### cmp to be killed. Not good. Later.
          if (Radix < 128)
            _indices2[_offset[Radix]++] = _indices[i]; // Number is positive, same as above
          else
            _indices2[--_offset[Radix]] = _indices[i]; // Number is negative, flip the sorting order
        }
        // Swap pointers for next pass. Valid indices - the most recent ones - are in _indices after the swap.
        uint32_t* Tmp = _indices;
        _indices      = _indices2;
        _indices2     = Tmp;
      } else {
        // The pass is useless, yet we still have to reverse the order of current list if all values are negative.
        if (UniqueVal >= 128) {
          for (uint32_t i = 0; i < nb; i++)
            _indices2[i] = _indices[nb - i - 1];

          // Swap pointers for next pass. Valid indices - the most recent ones - are in _indices after the swap.
          uint32_t* Tmp = _indices;
          _indices      = _indices2;
          _indices2     = Tmp;
        }
      }
    }
  }

  return *this;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 *	Resets the inner indices. After the call, _indices is reset.
 */
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void RadixSort::resetIndices() {
  for (uint32_t i = 0; i < _currentSize; i++)
    _indices[i] = i;
}

} // namespace ork
