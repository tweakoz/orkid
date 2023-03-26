////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <cstring>
#include <ork/application/application.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/reflect/Serialize.h>

#define COMPARE(op, y) compare(y) op 0

namespace ork {

PoolString::PoolString()
    : _stringptr(nullptr) {
}

PoolString::PoolString(const char* string)
    : _stringptr(string) {
}

PoolString::operator PieceString() const {
  return PieceString(c_str());
}
PoolString::operator ConstString() const {
  return ConstString(c_str());
}

bool PoolString::operator==(const PoolString& other) const {
  return COMPARE(==, other);
}

bool PoolString::operator<(const PoolString& other) const {
  return COMPARE(<, other);
}

bool PoolString::operator<=(const PoolString& other) const {
  return COMPARE(<=, other);
}

bool PoolString::operator>=(const PoolString& other) const {
  return COMPARE(>=, other);
}

bool PoolString::operator>(const PoolString& other) const {
  return COMPARE(>, other);
}

bool PoolString::operator!=(const PoolString& other) const {
  return COMPARE(!=, other);
}

PoolString::operator bool() const {
  return _stringptr != NULL;
}

///////////////////////////////////////////////////////////

bool PoolString::empty() const {
  return NULL == _stringptr || _stringptr[0] == '\0';
}

///////////////////////////////////////////////////////////

int PoolString::compare(const PoolString& rhs) const {
  if (_stringptr == rhs._stringptr)
    return 0;
  else if (_stringptr == NULL)
    return -1;
  else if (rhs._stringptr == NULL)
    return 1;
  else
    return strcmp(_stringptr, rhs._stringptr);
}

///////////////////////////////////////////////////////////

StringPool::StringPool(const StringPool* parent)
    : mStringPool()
    , mStringPoolMutex("StringPoolMutex")
    , _parent(parent) {
  // mStringPool.reserve( 2<<20 );
}

const char* StringPool::FindRecursive(const PieceString& string) const {
  const char* result = NULL;
  Lock();
  {
    bool search_result;
    orkvector<const char*>::size_type pos = BinarySearch(string, search_result);

    if (search_result)
      result = mStringPool[pos];
    else if (_parent)
      result = _parent->FindRecursive(string);
  }
  UnLock();
  return result;
}

const char* StringPool::FindFirst(const PieceString& string, VecType::iterator& it) {
  const char* result = NULL;
  Lock();
  {
    bool search_result;
    volatile VecType::size_type pos = BinarySearch(string, search_result);

    if (search_result)
      result = mStringPool[pos];
    else if (_parent)
      result = _parent->FindRecursive(string);

    OrkHeapCheck();

    if (NULL == result) {
      if (bool bpastend = (pos >= mStringPool.size()))
        it = mStringPool.end();
      else
        it = mStringPool.begin() + pos;
    }
  }
  UnLock();
  return result;
}

int StringPool::FindIndex(const PieceString& string) const {
  orkvector<const char*>::size_type pos;
  bool search_result = false;
  Lock();
  {
    OrkAssert(_parent == NULL);

    pos = BinarySearch(string, search_result);
  }
  UnLock();

  return search_result ? int(pos) : -1;
}

int StringPool::StringIndex(const PieceString& string) {
  PoolString pooled_string = String(string);

  return FindIndex(pooled_string);
}

int StringPool::LiteralIndex(const ConstString& string) {
  PoolString pooled_string = Literal(string);

  return FindIndex(pooled_string);
}

int StringPool::Size() const {
  return int(mStringPool.size());
}

PoolString StringPool::FromIndex(int index) const {
  OrkAssert(_parent == NULL);

  if (index == -1 || index >= int(mStringPool.size()))
    return PoolString();
  else
    return mStringPool[orkvector<const char*>::size_type(index)];
}

StringPool::VecType::size_type StringPool::BinarySearch(const PieceString& string, bool& result) const {
  VecType::size_type lo = 0;
  VecType::size_type hi = mStringPool.size();

  while (lo < hi) {
    orkvector<const char*>::size_type mid = (lo + hi) / 2;
    int cmp                               = string.compare(mStringPool[mid]);

    if (cmp < 0)
      hi = mid;
    else if (cmp > 0)
      lo = mid + 1;
    else {
      result = true;
      return mid;
    }
  }

  result = false;
  return lo;
}

PoolString StringPool::String(const PieceString& s) {
  const char* string = 0;
  Lock();
  {
    VecType::iterator insertion_point;
    string = FindFirst(s, insertion_point);
    if (string == NULL) {
      char* new_string = new char[s.length() + 1];
      std::memcpy(new_string, s.c_str(), s.length());
      new_string[s.length()] = '\0';

      string = new_string;
      mStringPool.insert(insertion_point, string);
    }
  }
  UnLock();
  return PoolString(string);
}

PoolString StringPool::Literal(const ConstString& s) {
  const char* string = 0;
  Lock();
  {
    VecType::iterator insertion_point = mStringPool.end();
    string                            = FindFirst(s, insertion_point);
    if (string == NULL) {
      string = s.c_str();
      mStringPool.insert(insertion_point, string);
    }
  }
  UnLock();
  return PoolString(string);
}

PoolString StringPool::Find(const PieceString& s) const {
  return FindRecursive(s);
}

///////////////////////////////////////////////////
/*
namespace reflect {
template <> void Serialize<PoolString>(const PoolString* in, PoolString* out, BidirectionalSerializer& bidi) {
  if (bidi.Serializing()) {
    bidi.Serializer()->serializeElement(PieceString(*in));
  } else {
    ArrayString<2048> buffer;
    MutableString string(buffer);
    // bidi.Deserializer()->deserialize(string);
    *out = ork::AddPooledString(string);
  }
}
} // namespace ork
*/
} // namespace ork
