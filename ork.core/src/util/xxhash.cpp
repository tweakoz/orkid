#include <ork/util/xxhash.inl>

namespace ork {

  XXH64HASH::XXH64HASH() {
    _state = XXH64_createState();
    init();
  }
  XXH64HASH::~XXH64HASH() {
    XXH64_freeState(_state);
  }

  void XXH64HASH::init() {
    XXH64_reset(_state, 0);
  }
  void XXH64HASH::finish() {
    _digest = XXH64_digest(_state);
  }

  uint64_t XXH64HASH::result() const {
    return _digest;
  }

  void XXH64HASH::accumulate(const void* data, size_t len) {
    XXH64_update(_state, data, len);
  }

  void XXH64HASH::accumulateString(const std::string& item) {
    accumulate(item.c_str(), item.length());
  }

  ///////////////////////////////////////////////////////////////////////////////

  XXH3HASH::XXH3HASH() {
    _state = XXH3_createState();
  }
  XXH3HASH::~XXH3HASH() {
    XXH3_freeState(_state);
  }

  void XXH3HASH::init() {
    XXH3_64bits_reset(_state);
  }
  void XXH3HASH::finish() {
    _digest = XXH3_64bits_digest(_state);
  }

  uint64_t XXH3HASH::result() const {
    return _digest;
  }

  void XXH3HASH::accumulate(const void* data, size_t len) {
    XXH3_64bits_update(_state, data, len);
  }

  void XXH3HASH::accumulateString(const std::string& item) {
    accumulate(item.c_str(), item.length());
  }

}