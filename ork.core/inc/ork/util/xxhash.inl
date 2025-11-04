#pragma once

#include <memory>
#include <string>
#include <vector>

#if !defined(ORK_IOS)
#include <xxhash.h>
#else
#include <ork/util/xxhash64_impl.inl>
#endif

namespace ork {

struct XXH64HASH {

  XXH64HASH();
  ~XXH64HASH();
  void init();
  void finish();
  uint64_t result() const;
  void accumulate(const void* data, size_t len);
  void accumulateString(const std::string& item);

  template <typename T> void accumulateItem(const T& item) {
    accumulate(&item, sizeof(T));
  }

#if !defined(ORK_IOS)
  XXH64_state_t* _state = nullptr;
#else
  std::vector<uint8_t> _buffer; // Buffer for iOS incremental hashing
#endif
  uint64_t _digest = 0xffffffffffffffff;
};

using xxh64hasher_ptr_t = std::shared_ptr<XXH64HASH>;

struct XXH3HASH {

  XXH3HASH();
  ~XXH3HASH();
  void init();
  void finish();
  uint64_t result() const;
  void accumulate(const void* data, size_t len);
  void accumulateString(const std::string& item);

  template <typename T> void accumulateItem(const T& item) {
    accumulate(&item, sizeof(T));
  }

#if !defined(ORK_IOS)
  XXH3_state_t* _state = nullptr;
#else
  std::vector<uint8_t> _buffer; // Buffer for iOS (uses XXH64 fallback)
#endif
  uint64_t _digest = 0xffffffffffffffff;
};

using xxh3hasher_ptr_t = std::shared_ptr<XXH3HASH>;
}