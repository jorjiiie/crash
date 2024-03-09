#pragma once

#ifndef LINEAR_HPP
#define LINEAR_HPP

#include <optional>
#include <vector>

#include "common.hpp"

namespace crash {

template <class Key, class Value>
  requires Hashable<Key>
class linear {

public:
  using K = Key;
  using V = Value;
  linear(size_t size_ = 16)
      : capacity(size_), keys(size_), values(size_), meta(2 * size_) {}

  std::optional<V> get(const K &k) const {
    size_t h = k.hash() & (capacity - 1);
    bool res;
    for (res = false;
         (meta[2 * h] || meta[2 * h + 1]) && (res = (k != keys[h]));) {
      h = (h + 1) & (capacity - 1);
    }
    if (meta[2 * h]) {
      return values[h];
    }
    return {};
  }

  V find(const K &k) const {
    size_t h = k.hash() & (capacity - 1);
    while ((meta[2 * h] || meta[2 * h + 1]) && (k != keys[h])) {
      h = (h + 1) & (capacity - 1);
    }
    return values[h];
  }
  void put(const K &k, V v) {
    size_t h = k.hash() & (capacity - 1);
    while ((meta[2 * h] || meta[2 * h + 1]) && (k != keys[h])) {
      h = (h + 1) & (capacity - 1);
    }
    if (meta[2 * h]) {
      // occupied, so its the value is here?
      values[h] = v;
      return;
    }

    sz++;
    effective_size++;
    meta[2 * h] = true;
    meta[2 * h + 1] = false;
    keys[h] = k;
    values[h] = v;
    if (effective_size * 2 > capacity) {
      linear replacement(2 * capacity);
      for (int i = 0; i < capacity; i++) {
        if (meta[2 * i]) {
          replacement.put(keys[i], values[i]);
        }
      }
      std::swap(replacement, *this);
    }
  }
  void erase(const K &k) {
    size_t h = k.hash() & (capacity - 1);
    while ((meta[2 * h] || meta[2 * h + 1]) && (k != keys[h])) {
      h = (h + 1) & (capacity - 1);
    }
    if (meta[2 * h]) {
      meta[2 * h] = false;
      meta[2 * h + 1] = true;
      sz--;
    }
  }

  void clear() {
    sz = 0;
    effective_size = 0;
    keys.clear();
    values.clear();
    meta.clear();
  }

  size_t prefetch(const K &k) {
    size_t h = k.hash() & (capacity - 1);
    return h;
  }
  size_t size() const { return sz; }
  uint64_t memuse() const {
    return sizeof(K) * keys.size() + sizeof(V) * values.size() +
           sizeof(bool) * meta.size() + sizeof(size_t) * 3;
  }

private:
  size_t sz = 0;
  size_t effective_size = 0;
  size_t capacity;
  std::vector<K> keys;
  std::vector<V> values;
  std::vector<bool> meta;
};
} // namespace crash

#endif
