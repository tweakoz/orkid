////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// Tweaks Look Up Table (sorted for O(Log n) search performance;

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/orkalloc.h>
#include <vector>
#include <utility>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

enum EOrkLutKeyPolicy { EKEYPOLICY_LUT = 0, EKEYPOLICY_MULTILUT };

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V> struct LutComparator {
  bool operator()(const std::pair<K, V>& a, const std::pair<K, V>& b) { return (a.first < b.first); }
};

///////////////////////////////////////////////////////////////////////////////

template <typename K, typename V, typename Allocator = std::allocator<std::pair<K, V>>> class orklut {
  EOrkLutKeyPolicy meKeyPolicy;

public:
  using base_t = std::vector<std::pair<K, V>, Allocator>;

  orklut(EOrkLutKeyPolicy ekeypolicy = EKEYPOLICY_LUT) : meKeyPolicy(ekeypolicy), _internal(0) {}

  typedef typename base_t::iterator iterator;
  typedef typename base_t::const_iterator const_iterator;
  typedef typename base_t::reverse_iterator reverse_iterator;
  typedef typename base_t::const_reverse_iterator const_reverse_iterator;

  typedef K key_type;
  typedef V mapped_type;
  typedef typename base_t::value_type value_type;
  typedef typename base_t::size_type size_type;
  typedef typename base_t::difference_type difference_type;

  // ork::_vector_iterator_base<Data,Traverse,Const> ork::orklut<K,V>::BinarySearch(const K
  // &,ork::_vector_iterator_base<Data,Traverse,Const> &) const

  EOrkLutKeyPolicy GetKeyPolicy() const { return meKeyPolicy; }
  void SetKeyPolicy(EOrkLutKeyPolicy policy) { meKeyPolicy = policy; }

  // iterator BinarySearchNonConst( const K & Testkey, iterator & insertion_point );
  // const_iterator BinarySearch( const K & Testkey, const_iterator & insertion_point ) const;
  const_iterator LowerBound(const K& Testkey) const;
  const_iterator UpperBound(const K& Testkey) const;
  iterator LowerBound(const K& Testkey);
  iterator UpperBound(const K& Testkey);

  const_iterator begin() const { return _internal.begin(); }
  const_iterator end() const { return _internal.end(); }
  iterator begin() { return _internal.begin(); }
  iterator end() { return _internal.end(); }
  // using base_t::end;
  const_reverse_iterator rbegin() const { return _internal.rbegin(); }
  const_reverse_iterator rend() const { return _internal.rend(); }
  reverse_iterator rbegin() { return _internal.rbegin(); }
  reverse_iterator rend() { return _internal.rend(); }
  // using base_t::rbegin;
  // using base_t::rend;
  void clear() { _internal.clear(); }
  size_t size() const { return _internal.size(); }
  void reserve(size_t cnt) { _internal.reserve(cnt); }
  // using base_t::clear;
  // using base_t::size;
  // using base_t::reserve;

  void Clear() { this->clear(); }
  void Replace(const K& key, const V& val);
  iterator AddSorted(const K& key, const V& val);
  void insert(const value_type& kv) { AddSorted(kv.first, kv.second); };
  const_iterator find(const K& key) const;
  iterator find(const K& key);
  void RemoveItem(const iterator& it);
  void erase(const iterator& it) { return RemoveItem(it); }

  bool Empty() const { return _internal.empty(); }
  size_t Size() const { return _internal.size(); }
  const value_type& GetItemAtIndex(size_t idx) const { return _internal[size_type(idx)]; }
  const_iterator GetIterAtIndex(size_t idx) const { return _internal.begin() + size_type(idx); }
  typename base_t::value_type& RefIndex(size_t idx) { return _internal[size_type(idx)]; }

private:
  base_t _internal;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
