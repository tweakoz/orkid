////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.h>
#include <assert.h>
#include <algorithm>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::const_iterator orklut<K, V, Allocator>::LowerBound(const K& key) const {
  if (Empty())
    return end();

  value_type ComparePair(key, V());
  const_iterator WhereIt = std::lower_bound(begin(), end(), ComparePair, LutComparator<K, V>());
  return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::const_iterator orklut<K, V, Allocator>::UpperBound(const K& key) const {
  if (Empty())
    return end();

  value_type ComparePair(key, V());
  const_iterator WhereIt = std::upper_bound(begin(), end(), ComparePair, LutComparator<K, V>());
  return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::iterator orklut<K, V, Allocator>::LowerBound(const K& key) {
  if (Empty())
    return end();

  value_type ComparePair(key, V());
  iterator WhereIt = std::lower_bound(begin(), end(), ComparePair, LutComparator<K, V>());
  return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::iterator orklut<K, V, Allocator>::UpperBound(const K& key) {
  if (Empty())
    return end();

  value_type ComparePair(key, V());
  auto WhereIt = std::upper_bound(begin(), end(), ComparePair, LutComparator<K, V>());
  return WhereIt;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::iterator orklut<K, V, Allocator>::AddSorted(const K& key, const V& val) {

  value_type InsertPair(key, val);

  auto as_const = const_cast<const orklut<K, V, Allocator>*>(this);

  assert(
      (as_const->find(key) == as_const->end()) ||
      (meKeyPolicy == EKEYPOLICY_MULTILUT)); // "lut is a singlekey lut and can not have multiple items with the same key, use
                                             // EKEYPOLICY_MULTILUT if you want a multilut"

  iterator WhereIt = UpperBound(key);

  if (WhereIt == end()) // at the end
  {
    difference_type iter = difference_type(size());
    _internal.push_back(InsertPair);
    return _internal.end() - 1;
  } else // before WhereIt
  {
    return _internal.insert(WhereIt, InsertPair);
    // return WhereIt;
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator> void orklut<K, V, Allocator>::Replace(const K& key, const V& val) {
  iterator WhereIt = find(key);
  if (WhereIt == end()) {
    value_type InsertPair(key, val);

    WhereIt = UpperBound(key);

    if (WhereIt == end()) // at the end
    {
      difference_type iter = difference_type(size());
      _internal.push_back(InsertPair);
    } else // before WhereIt
    {
      _internal.insert(WhereIt, InsertPair);
    }
  } else // before WhereIt
  {
    WhereIt->second = val;
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::const_iterator orklut<K, V, Allocator>::find(const K& key) const {
  const_iterator it = LowerBound(key); // it.first >= key

  if (it != end()) {
    if (it->first != key) // key not present
    {
      it = _internal.end();
    }
  }

  return it;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
typename orklut<K, V, Allocator>::iterator orklut<K, V, Allocator>::find(const K& key) {
  iterator it = LowerBound(key); // it.first >= key

  if (it != end()) {
    if (it->first != key) // key not present
    {
      it = _internal.end();
    }
  }
  return it;
}

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator>
void orklut<K, V, Allocator>::RemoveItem(typename orklut<K, V, Allocator>::iterator const& it) {
  _internal.erase(it);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
#define ImplementOrkLut(key_type,value_type) \
namespace ork { \
  template class orklut<key_type, value_type>; \
} // namespace ork