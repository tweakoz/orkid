///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//	contains source code from the article "Radix Sort Revisited".
//	author		Pierre Terdiman
//	date		April, 4, 2000
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <string.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: Fix this

class RadixSort {
public:
  // Constructor/Destructor
  RadixSort();
  ~RadixSort();
  // Sorting methods
  RadixSort& Sort(const uint32_t* input, size_t nb, bool signedvalues = true);
  RadixSort& Sort(const float* input, size_t nb);

  //! Access to results. _indices is a list of indices in sorted order, i.e. in the order you may further process your data
  inline uint32_t* GetIndices() const {
    return _indices;
  }

  //! Returns the total number of calls to the radix sorter.
  inline uint32_t GetNbTotalCalls() const {
    return _totalCalls;
  }
  //! Returns the number of premature exits due to temporal coherence.
  inline uint32_t GetNbHits() const {
    return _numBhits;
  }

  RadixSort(const RadixSort& object);
  RadixSort& operator=(const RadixSort& object);

  uint32_t currentSize() const {
    return _currentSize;
  }

  static constexpr size_t HISTOSIZE = 1024;

  uint32_t* _histogram   = nullptr; //!< Counters for each byte
  uint32_t* _offset      = nullptr; //!< Offsets (nearly a cumulative distribution function)
  uint32_t _currentSize  = 0;       //!< Current size of the indices list
  uint32_t _previousSize = 0;       //!< Size involved in previous call
  uint32_t* _indices     = nullptr; //!< Two lists, swapped each pass
  uint32_t* _indices2    = nullptr;
  // Stats
  uint32_t _totalCalls = 0;
  uint32_t _numBhits   = 0;
  // Internal methods
  void resetIndices();

  void CHECK_RESIZE(size_t n);

  template <typename type> bool CREATE_HISTOGRAMS(const type* buffer, const size_t nb) {
    /* Clear counters */
    ::memset((void*)this->_histogram, 0, HISTOSIZE * sizeof(uint32_t));

    /* Prepare for temporal coherence */
    type PrevVal       = type(buffer[_indices[0]]);
    bool AlreadySorted = true; /* Optimism... */
    uint32_t* Indices  = _indices;

    /* Prepare to count */
    auto p       = (uint8_t*)buffer;
    uint8_t* pe  = &p[nb * 4];
    uint32_t* h0 = &_histogram[0];   /* Histogram for first pass (LSB)       */
    uint32_t* h1 = &_histogram[256]; /* Histogram for second pass            */
    uint32_t* h2 = &_histogram[512]; /* Histogram for third pass                     */
    uint32_t* h3 = &_histogram[768]; /* Histogram for last pass (MSB)        */

    while (p != pe) {
      /* Read input buffer in previous sorted order */
      uint32_t Val = (uint32_t)buffer[*Indices++];
      /* Check whether already sorted or not */
      if (type(Val) < PrevVal) {
        AlreadySorted = false;
        break;
      } /* Early out */
      /* Update for next iteration */
      PrevVal = type(Val);

      /* Create histograms */
      h0[*p++]++;
      h1[*p++]++;
      h2[*p++]++;
      h3[*p++]++;
    }
    /* If all input values are already sorted, we just have to return and leave the */
    /* previous list unchanged. That way the routine may take advantage of temporal */
    /* coherence, for example when used to sort transparent faces.                                  */
    if (AlreadySorted) {
      _numBhits++;
      return true;
    }

    /* Else there has been an early out and we must finish computing the histograms */
    while (p != pe) {
      /* Create histograms without the previous overhead */
      h0[*p++]++;
      h1[*p++]++;
      h2[*p++]++;
      h3[*p++]++;
    }

    return AlreadySorted;
  }
};

} // namespace ork
