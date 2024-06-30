#pragma once

#include <vector>
#include <unordered_map>

namespace ork {

template <typename T>
struct fast_set {
  std::unordered_map<T, size_t> _uset;
  std::vector<T> _linear;

  void insert(T v);
  void remove(T v);
};

template <typename T>
void fast_set<T>::insert(T v) {
  if (_uset.find(v) == _uset.end()) {  // Prevent duplicates
    size_t index = _linear.size();
    _linear.push_back(v);
    _uset[v] = index;
  }
}

template <typename T>
void fast_set<T>::remove(T v) {
  auto it = _uset.find(v);
  if (it != _uset.end()) {
    size_t index = it->second;
    _uset.erase(it);
    size_t count = _linear.size();
    if (index < count - 1) {
      _linear[index] = _linear[count - 1];
      _uset[_linear[index]] = index;  // Update the moved element's index
    }
    _linear.pop_back();
  }
}

} // namespace ork
