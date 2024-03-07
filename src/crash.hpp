#pragma once

#ifndef CRASH_HPP
#define CRASH_HPP

#include <cstring>
#include <functional>
#include <optional>
#include <vector>

#include <iostream>
#include <set>
#include <string>

#include "common.hpp"

namespace crash {

template <class Key, class Value>
  requires Hashable<Key>
class hashtable {
public:
  using K = Key;
  using V = Value;

  hashtable(size_t size = 16)
      : keys(size), values(size), metadata(2 * size, false),
        current_size(size){};
  ~hashtable() = default;

  std::optional<const std::reference_wrapper<const V>> get(const K &key) const {
    // use quadratic probing
    size_t h = get_slot(key);
    if (metadata[2 * h]) {
      return values[h];
    }
    return {};
  }

  void put(const K &key, V v) {

    size_t h = get_slot(key);

    if (metadata[2 * h]) {
      values[h] = v;
      return;
    }

    // insert
    keys[h] = key;

    metadata[2 * h] = true;
    metadata[2 * h + 1] = false;

    values[h] = v;

    num_keys++;
    effective_keys++;

    if (effective_keys * 2 > current_size) {
      hashtable x(current_size * 2);
      int p = 0;
      for (int i = 0; i < current_size; i++) {
        p++;
        if (metadata[2 * i]) {
          x.put(keys[i], values[i]);
        }
      }
      *this = std::move(x);
    }
  }

  bool erase(const K &key) {
    size_t h = get_slot(key);
    if (metadata[2 * h]) {
      num_keys--;
      metadata[2 * h] = false;
      metadata[2 * h + 1] = true;

      return true;
    }
    return false;
  }
  size_t size() const { return num_keys; }
  size_t capacity() const { return current_size; }

  void dump() const {
    for (int i = 0; i < current_size; i++) {
      if (metadata[2 * i]) {
        std::cerr << i << ": " << keys[i] << " " << values[i] << "\n";
      }
    }
  }

private:
  [[nodiscard]] size_t get_slot(const K &key) const {
    size_t h = key.hash() & (keys.size() - 1);
    int i = 1;
    while ((metadata[2 * i] || metadata[2 * i + 1]) && keys[i] <=> key != 0) {
      h += i * (i + 1) / 2;
      i++;
      h &= (current_size - 1);
    }
    return h;
  }
  size_t num_keys = 0;
  size_t effective_keys = 0;
  size_t current_size = 16;
  std::vector<K> keys;
  std::vector<Value> values;
  std::vector<bool>
      metadata; // meta[2i] = occupied[i], meta[2i+1] = tombstone[i]
};

} // namespace crash

#endif
