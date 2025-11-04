#include <ork/util/xxhash.inl>

namespace ork {

#if !defined(ORK_IOS)
  // Full xxhash implementation for non-iOS platforms
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

#else
  // iOS implementation using header-only xxhash64_impl.inl
  XXH64HASH::XXH64HASH() {
    init();
  }
  XXH64HASH::~XXH64HASH() {}

  void XXH64HASH::init() {
    _buffer.clear();
    _digest = 0;
  }

  void XXH64HASH::finish() {
    // Compute hash from buffered data using header-only implementation
    if (_buffer.empty()) {
      _digest = xxh64::hash("", 0, 0);
    } else {
      _digest = xxh64::hash(reinterpret_cast<const char*>(_buffer.data()), _buffer.size(), 0);
    }
  }

  uint64_t XXH64HASH::result() const {
    return _digest;
  }

  void XXH64HASH::accumulate(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    _buffer.insert(_buffer.end(), bytes, bytes + len);
  }

  void XXH64HASH::accumulateString(const std::string& item) {
    accumulate(item.c_str(), item.length());
  }

  ///////////////////////////////////////////////////////////////////////////////

  XXH3HASH::XXH3HASH() {
    init();
  }
  XXH3HASH::~XXH3HASH() {}

  void XXH3HASH::init() {
    _buffer.clear();
    _digest = 0;
  }

  void XXH3HASH::finish() {
    // Use XXH64 as fallback for XXH3 on iOS
    if (_buffer.empty()) {
      _digest = xxh64::hash("", 0, 0);
    } else {
      _digest = xxh64::hash(reinterpret_cast<const char*>(_buffer.data()), _buffer.size(), 0);
    }
  }

  uint64_t XXH3HASH::result() const {
    return _digest;
  }

  void XXH3HASH::accumulate(const void* data, size_t len) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    _buffer.insert(_buffer.end(), bytes, bytes + len);
  }

  void XXH3HASH::accumulateString(const std::string& item) {
    accumulate(item.c_str(), item.length());
  }
#endif

}