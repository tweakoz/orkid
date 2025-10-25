////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <vector>
#include <ork/pch.h>
///////////////////////////////////////////////////////////////////////////////

namespace ork {

///////////////////////////////////////////////////////////////////////////////
// priority_stack: Stack with O(1) highest-priority query
//
// Requirements:
//   - T must have member: int _priority (default 0)
//
// Performance:
//   - push():    O(1) amortized (vector push_back + cache update)
//   - pop():     O(1)
//   - resolve(): O(1) when cached, O(n) on cache miss (amortized O(1))
//   - Stack semantics: LIFO (Last In, First Out)
//
// Cache Strategy:
//   - Maintains cached highest-priority element
//   - Cache invalidated on pop() for safety
//   - Cache updated on push() if new item is higher priority
//   - Cache recomputed lazily on resolve() if invalid
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class priority_stack {
public:

  priority_stack() = default;

  ///////////////////////////////////////////////////////////////////////////////
  // Push item onto stack (LIFO)
  // Updates cache if new item has higher or equal priority
  ///////////////////////////////////////////////////////////////////////////////
  void push(const T& item) {
    _stack.push_back(item);

    // Update cache if this is higher or equal priority
    // Use >= so that equal-priority items follow LIFO (later push wins)
    if (!_cache_valid || item._priority >= _cached_max._priority) {
      _cached_max = item;
      _cache_valid = true;
    }
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Pop most recently pushed item (LIFO)
  // Invalidates cache for correctness
  ///////////////////////////////////////////////////////////////////////////////
  void pop() {
    OrkAssert(!_stack.empty());
    _stack.pop_back();

    // Invalidate cache - it will be recomputed on next resolve()
    _invalidate_cache();
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Return highest priority item currently on stack
  // O(1) if cached, O(n) on cache miss
  ///////////////////////////////////////////////////////////////////////////////
  T resolve() const {
    if (!_cache_valid) {
      _update_cache();
    }
    return _cached_max;
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Query operations
  ///////////////////////////////////////////////////////////////////////////////

  bool empty() const {
    return _stack.empty();
  }

  size_t size() const {
    return _stack.size();
  }

  const T& top() const {
    OrkAssert(!_stack.empty());
    return _stack.back();
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Direct access (use with caution - invalidates cache)
  ///////////////////////////////////////////////////////////////////////////////

  void clear() {
    _stack.clear();
    _invalidate_cache();
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Debug: dump priorities in stack
  ///////////////////////////////////////////////////////////////////////////////
  void dump(const char* label = "") const {
    printf("priority_stack<%s> size<%zu>: [", label, _stack.size());
    for(size_t i = 0; i < _stack.size(); ++i) {
      if(i > 0) printf(", ");
      printf("%d", _stack[i]._priority);
    }
    printf("]\n");
  }

private:

  ///////////////////////////////////////////////////////////////////////////////
  // Invalidate cache (called on pop/clear)
  ///////////////////////////////////////////////////////////////////////////////
  void _invalidate_cache() const {
    _cache_valid = false;
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Recompute highest priority item by scanning stack
  // Called lazily on resolve() when cache is invalid
  ///////////////////////////////////////////////////////////////////////////////
  void _update_cache() const {
    if (_stack.empty()) {
      _cache_valid = false;
      return;
    }

    // Scan stack for highest priority
    _cached_max = _stack[0];
    for (size_t i = 1; i < _stack.size(); ++i) {
      if (_stack[i]._priority > _cached_max._priority) {
        _cached_max = _stack[i];
      }
    }
    _cache_valid = true;
  }

  ///////////////////////////////////////////////////////////////////////////////
  // Data members
  ///////////////////////////////////////////////////////////////////////////////

  std::vector<T> _stack;               // LIFO stack storage
  mutable T _cached_max;               // Cached highest-priority element
  mutable bool _cache_valid = false;   // Cache validity flag
};

///////////////////////////////////////////////////////////////////////////////
// Specialization for raw pointers: priority_stack<T*>
// Accesses _priority from pointed-to object
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class priority_stack<T*> {
public:

  priority_stack() = default;

  void push(T* item) {
    _stack.push_back(item);

    // Update cache if this is higher or equal priority (skip nullptr)
    // Use >= so that equal-priority items follow LIFO (later push wins)
    if (item != nullptr) {
      if (!_cache_valid || _cached_max == nullptr || item->_priority >= _cached_max->_priority) {
        _cached_max = item;
        _cache_valid = true;
      }
    }
  }

  void pop() {
    OrkAssert(!_stack.empty());
    _stack.pop_back();
    _invalidate_cache();
  }

  T* resolve() const {
    if (!_cache_valid) {
      _update_cache();
    }
    return _cached_max;
  }

  bool empty() const {
    return _stack.empty();
  }

  size_t size() const {
    return _stack.size();
  }

  T* top() const {
    OrkAssert(!_stack.empty());
    return _stack.back();
  }

  void clear() {
    _stack.clear();
    _invalidate_cache();
  }

  void dump(const char* label = "") const {
    printf("priority_stack<%s> size<%zu>: [", label, _stack.size());
    for(size_t i = 0; i < _stack.size(); ++i) {
      if(i > 0) printf(", ");
      if(_stack[i] != nullptr) {
        printf("%d", _stack[i]->_priority);
      } else {
        printf("null");
      }
    }
    printf("]\n");
  }

private:

  void _invalidate_cache() const {
    _cache_valid = false;
  }

  void _update_cache() const {
    if (_stack.empty()) {
      _cache_valid = false;
      _cached_max = nullptr;
      return;
    }

    // Find first non-null pointer
    _cached_max = nullptr;
    for (size_t i = 0; i < _stack.size(); ++i) {
      if (_stack[i] != nullptr) {
        _cached_max = _stack[i];
        break;
      }
    }

    // Scan remaining for highest priority (skip nullptrs)
    for (size_t i = 0; i < _stack.size(); ++i) {
      if (_stack[i] != nullptr) {
        if (_cached_max == nullptr || _stack[i]->_priority > _cached_max->_priority) {
          _cached_max = _stack[i];
        }
      }
    }

    _cache_valid = true;
  }

  std::vector<T*> _stack;
  mutable T* _cached_max = nullptr;
  mutable bool _cache_valid = false;
};

///////////////////////////////////////////////////////////////////////////////
// Specialization for shared_ptr: priority_stack<std::shared_ptr<T>>
// Accesses _priority from pointed-to object
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class priority_stack<std::shared_ptr<T>> {
public:

  using ptr_t = std::shared_ptr<T>;

  priority_stack() = default;

  void push(const ptr_t& item) {
    _stack.push_back(item);

    // Update cache if this is higher or equal priority (skip nullptr)
    // Use >= so that equal-priority items follow LIFO (later push wins)
    if (item != nullptr) {
      if (!_cache_valid || _cached_max == nullptr || item->_priority >= _cached_max->_priority) {
        _cached_max = item;
        _cache_valid = true;
      }
    }
  }

  void pop() {
    OrkAssert(!_stack.empty());
    _stack.pop_back();
    _invalidate_cache();
  }

  ptr_t resolve() const {
    if (!_cache_valid) {
      _update_cache();
    }
    return _cached_max;
  }

  bool empty() const {
    return _stack.empty();
  }

  size_t size() const {
    return _stack.size();
  }

  const ptr_t& top() const {
    OrkAssert(!_stack.empty());
    return _stack.back();
  }

  void clear() {
    _stack.clear();
    _invalidate_cache();
  }

  void dump(const char* label = "") const {
    printf("priority_stack<%s> size<%zu>: [", label, _stack.size());
    for(size_t i = 0; i < _stack.size(); ++i) {
      if(i > 0) printf(", ");
      if(_stack[i] != nullptr) {
        printf("%d", _stack[i]->_priority);
      } else {
        printf("null");
      }
    }
    printf("]\n");
  }

private:

  void _invalidate_cache() const {
    _cache_valid = false;
  }

  void _update_cache() const {
    if (_stack.empty()) {
      _cache_valid = false;
      _cached_max = nullptr;
      return;
    }

    // Find first non-null pointer
    _cached_max = nullptr;
    for (size_t i = 0; i < _stack.size(); ++i) {
      if (_stack[i] != nullptr) {
        _cached_max = _stack[i];
        break;
      }
    }

    // Scan remaining for highest priority (skip nullptrs)
    for (size_t i = 0; i < _stack.size(); ++i) {
      if (_stack[i] != nullptr) {
        if (_cached_max == nullptr || _stack[i]->_priority > _cached_max->_priority) {
          _cached_max = _stack[i];
        }
      }
    }

    _cache_valid = true;
  }

  std::vector<ptr_t> _stack;
  mutable ptr_t _cached_max;
  mutable bool _cache_valid = false;
};

///////////////////////////////////////////////////////////////////////////////
// Specialization for weak_ptr: priority_stack<std::weak_ptr<T>>
// Accesses _priority from pointed-to object (locks weak_ptr first)
///////////////////////////////////////////////////////////////////////////////

template <typename T>
class priority_stack<std::weak_ptr<T>> {
public:

  using weak_ptr_t = std::weak_ptr<T>;
  using shared_ptr_t = std::shared_ptr<T>;

  priority_stack() = default;

  void push(const weak_ptr_t& item) {
    _stack.push_back(item);

    // Update cache if this is higher or equal priority (skip expired weak_ptrs)
    // Use >= so that equal-priority items follow LIFO (later push wins)
    auto locked = item.lock();
    if (locked != nullptr) {
      if (!_cache_valid) {
        _cached_max = item;
        _cache_valid = true;
      } else {
        auto cached_locked = _cached_max.lock();
        if (!cached_locked || locked->_priority >= cached_locked->_priority) {
          _cached_max = item;
        }
      }
    }
  }

  void pop() {
    OrkAssert(!_stack.empty());
    _stack.pop_back();
    _invalidate_cache();
  }

  weak_ptr_t resolve() const {
    if (!_cache_valid) {
      _update_cache();
    }
    return _cached_max;
  }

  bool empty() const {
    return _stack.empty();
  }

  size_t size() const {
    return _stack.size();
  }

  const weak_ptr_t& top() const {
    OrkAssert(!_stack.empty());
    return _stack.back();
  }

  void clear() {
    _stack.clear();
    _invalidate_cache();
  }

  void dump(const char* label = "") const {
    printf("priority_stack<%s> size<%zu>: [", label, _stack.size());
    for(size_t i = 0; i < _stack.size(); ++i) {
      if(i > 0) printf(", ");
      auto locked = _stack[i].lock();
      if(locked) {
        printf("%d", locked->_priority);
      } else {
        printf("expired");
      }
    }
    printf("]\n");
  }

private:

  void _invalidate_cache() const {
    _cache_valid = false;
  }

  void _update_cache() const {
    if (_stack.empty()) {
      _cache_valid = false;
      return;
    }

    // Find highest priority, skipping expired weak_ptrs
    _cached_max = _stack[0];
    auto max_locked = _cached_max.lock();

    for (size_t i = 1; i < _stack.size(); ++i) {
      auto current_locked = _stack[i].lock();
      if (!current_locked) continue; // Skip expired weak_ptr

      if (!max_locked || current_locked->_priority > max_locked->_priority) {
        _cached_max = _stack[i];
        max_locked = current_locked;
      }
    }

    _cache_valid = (max_locked != nullptr);
  }

  std::vector<weak_ptr_t> _stack;
  mutable weak_ptr_t _cached_max;
  mutable bool _cache_valid = false;
};

} // namespace ork
