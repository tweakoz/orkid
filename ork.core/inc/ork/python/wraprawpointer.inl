////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////


#pragma once

namespace ork::python {
template <class T> struct unmanaged_ptr {
  using value_type = T*;

  ~unmanaged_ptr(){
    // delete _ptr; NOP
  }

  unmanaged_ptr()
      : _ptr(nullptr) {
  }
  explicit unmanaged_ptr(T* ptr)
      : _ptr(ptr) {
  }
  unmanaged_ptr(const unmanaged_ptr& other)
      : _ptr(other._ptr) {
  }
  unmanaged_ptr(unmanaged_ptr&& other) noexcept : _ptr(other._ptr) {
    other._ptr = nullptr;
  }
  unmanaged_ptr& operator=(unmanaged_ptr&& other) noexcept {
    _ptr = other._ptr;
    other._ptr = nullptr;
    return *this;
  }

  operator bool() const {
    return _ptr != nullptr;
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
  void assign(const unmanaged_ptr& other) {
    _ptr = other._ptr;
  }
  T& operator[](std::size_t idx) const {
    return _ptr[idx];
  }

  T* _ptr;
};

template <class T> struct unmanaged_const_ptr {
  using value_type = const T*;

  ~unmanaged_const_ptr(){
    // delete _ptr; NOP
  }

  unmanaged_const_ptr()
      : _ptr(nullptr) {
  }
  explicit unmanaged_const_ptr(const T* ptr)
      : _ptr(ptr) {
  }
  unmanaged_const_ptr(const unmanaged_const_ptr& other)
      : _ptr(other._ptr) {
  }
  unmanaged_const_ptr(unmanaged_const_ptr&& other) noexcept : _ptr(other._ptr) {
    other._ptr = nullptr;
  }
  unmanaged_const_ptr& operator=(unmanaged_const_ptr&& other) noexcept {
    _ptr = other._ptr;
    other._ptr = nullptr;
    return *this;
  }

  operator bool() const {
    return _ptr != nullptr;
  }
  const T& operator*() const {
    return *_ptr;
  }
  const T* operator->() const {
    return _ptr;
  }
  const T* get() const {
    return _ptr;
  }
  const T& ref() const {
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
  void assign(const unmanaged_const_ptr& other) {
    _ptr = other._ptr;
  }
  T& operator[](std::size_t idx) const {
    return _ptr[idx];
  }

  const T* _ptr;
};

} // namespace ork::python

