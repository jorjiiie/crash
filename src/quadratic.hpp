#pragma once

#ifndef QUADRATIC_HPP
#define QUADRATIC_HPP

#include <cstring>
#include <functional>
#include <optional>
#include <vector>

#include <iostream>
#include <set>
#include <string>

#include "common.hpp"

namespace crash {

template <class Key, class Value, int LoadFactor>
  requires Hashable<Key>
class quadratic {
public:
  using K = Key;
  using V = Value;
  double LF = LoadFactor / 100.0;

  quadratic(size_t size_ = 16)
      : capacity(size_), keys(size_), values(size_), meta(2 * size_) {}

  std::optional<V> get(const K &k) const {
    size_t h = k.hash() & (capacity - 1); // save might save an instruction
    size_t i = 1;
    bool res;
    for (i = 1, res = false;
         (meta[2 * h] || meta[2 * h + 1]) && (res = (k != keys[h]));) {
      h = (h + i) & (capacity - 1);
      i++;
    }
    if (meta[2 * h]) {
      return values[h];
    }
    return {};
  }
  V find(const K &k) const {

    size_t h = k.hash() & (capacity - 1);
    size_t i = 1;
    for (i = 1; (meta[2 * h] || meta[2 * h + 1]) && (k != keys[h]);) {
      h = (h + i) & (capacity - 1);
      i++;
    }
    return values[h];
  }

  void put(const K &k, V v) {

    size_t h = k.hash() & (capacity - 1);
    size_t i = 1;
    for (; (meta[2 * h] || meta[2 * h + 1]) && (k != keys[h]);) {
      h = (h + i) & (capacity - 1);
      i++;
    }
    if (meta[2 * h]) {
      values[h] = v;
      return;
    }

    _size++;
    effective_size++;
    // do i resize? yes
    meta[2 * h] = true;
    meta[2 * h + 1] = false;
    keys[h] = k;
    values[h] = v;

    if (effective_size >= capacity * LF) {
      // resize

      quadratic replacement(2 * capacity);
      for (int i = 0; i < capacity; i++) {
        if (meta[2 * i]) {
          replacement.put(keys[i], values[i]);
        }
      }
      std::swap(*this, replacement);
    }
  }
  void erase(const K &k) {

    size_t h = k.hash() & (capacity - 1);
    size_t i = 1;
    bool res;
    for (i = 1, res = false;
         (meta[2 * h] || meta[2 * h + 1]) && (res = (k != keys[h]));) {
      h = (h + i) & (capacity - 1);
      i++;
    }
    if (res) {
      _size--;
      meta[2 * h] = false;
      meta[2 * h + 1] = true;
    }
  }
  void clear() {
    _size = effective_size = 0;
    keys.clear();
    values.clear();
    meta.clear();
  }
  size_t size() const { return _size; }
  uint64_t memuse() const {
    return keys.size() * sizeof(K) + values.size() * sizeof(V) + meta.size();
  }

private:
  size_t _size = 0;
  size_t effective_size = 0;
  size_t capacity;
  std::vector<K> keys;
  std::vector<V> values;
  std::vector<bool> meta;
};
} // namespace crash

#endif
