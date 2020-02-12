#pragma once

namespace ork::python {
template <class T> struct unmanaged_ptr {
  using value_type = T*;
  unmanaged_ptr()
      : _ptr(nullptr) {
  }
  unmanaged_ptr(T* ptr)
      : _ptr(ptr) {
  }
  unmanaged_ptr(const unmanaged_ptr& other)
      : _ptr(other._ptr) {
  }
  T& operator*() const {
    return *_ptr;
  }
  T* operator->() const {
    return _ptr;
  }
  T* get() const {
    return _ptr;
  }
  T& ref() const {
    OrkAssert(_ptr != nullptr);
    return *_ptr;
  }
  const T& const_ref() const {
    OrkAssert(_ptr != nullptr);
    return *_ptr;
  }
  void destroy() {
    // delete _ptr;
  }
  void deallocate() {
    // delete _ptr;
  }
  T& operator[](std::size_t idx) const {
    return _ptr[idx];
  }

  T* _ptr;
};

} // namespace ork::python
