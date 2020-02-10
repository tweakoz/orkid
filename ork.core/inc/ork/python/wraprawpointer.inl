#pragma once

namespace ork::python {
template <class T> class unmanaged_ptr {
public:
  unmanaged_ptr()
      : ptr(nullptr) {
  }
  unmanaged_ptr(T* ptr)
      : ptr(ptr) {
  }
  unmanaged_ptr(const unmanaged_ptr& other)
      : ptr(other.ptr) {
  }
  T& operator*() const {
    return *ptr;
  }
  T* operator->() const {
    return ptr;
  }
  T* get() const {
    return ptr;
  }
  void destroy() {
    // delete ptr;
  }
  T& operator[](std::size_t idx) const {
    return ptr[idx];
  }

private:
  T* ptr;
};

} // namespace ork::python
