#pragma once

#ifndef ROBINHOOD_HPP
#define ROBINHOOD_HPP

#include "common.hpp"

#include <iostream>

namespace crash {

template <class Key, class Value, int LoadFactor>
  requires Hashable<Key>
class robinhood {
public:
  using K = Key;
  using V = Value;
  double LF = LoadFactor / 100.0;

  robinhood(size_t size_ = 16)
      : capacity(size_), keys(size_), values(size_), occupied(size_) {}

  std::optional<V> get(const K &k) const {
    int probe_length;
    size_t h = k.hash() & (capacity - 1);
    bool res;
    for (probe_length = 0, res = false;
         occupied[h] && (res = (k != keys[h]));) {
      h = (h + 1) & (capacity - 1);
      probe_length++;
    }
    if (occupied[h])
      return values[h];
    return {};
  }
  V find(const K &k) const {
    int probe_length;
    size_t h = k.hash() & (capacity - 1);
    for (probe_length = 0; occupied[h] && (k != keys[h]);) {
      h = (h + 1) & (capacity - 1);
      probe_length++;
    }
    return values[h];
  }

  void put(const K &k_, V v) {
    if (effective_size >= capacity * LF) {
      // grow
      robinhood<K, V, LoadFactor> new_table(capacity * 2);
      for (int i = 0; i < capacity; i++) {
        if (occupied[i]) {
          new_table.put(keys[i], values[i]);
        }
      }
      std::swap(new_table, *this);
    }
    K k = k_;

    size_t h = k.hash() & (capacity - 1);
    size_t dist = 0;
    for (;;) {
      // what's here then - it's either a tombstone | occupied, and whatever
      if (occupied[h] == false) {
        _size++;
        effective_size++;
        occupied[h] = true;
        keys[h] = k;
        values[h] = v;
        return;
      }
      if (keys[h] == k) {
        values[h] = v;
        return;
      }
      // potentially swap
      // saves an instruction LOL
      size_t dist2 = (keys[h].hash() - h + capacity) & (capacity - 1);
      if (dist2 < dist) {
        std::swap(k, keys[h]);
        std::swap(v, values[h]);
        dist = dist2;
      }
      dist++;

      h = (h + 1) & (capacity - 1);
    }
  }

  void erase(const K &k) {
    size_t h = k.hash() & (capacity - 1);
    for (;;) {
      if (occupied[h] == false)
        return;
      if (keys[h] == k) {
        occupied[h] = false;
        _size--;

        // backwards shift
        size_t cur = (h + 1) & (capacity - 1);
        for (;;) {
          if (occupied[cur] == false)
            return;
          if ((keys[cur].hash() & (capacity - 1)) == cur) {
            // in optimal spot
            return;
          }
          keys[h] = keys[cur];
          values[h] = values[cur];
          occupied[cur] = false;
          h = cur;
          cur = (cur + 1) & (capacity - 1);
        }
        return;
      }
      h = (h + 1) & (capacity - 1);
    }
  }
  void clear() {
    _size = 0;
    effective_size = 0;
    keys.clear();
    values.clear();
    occupied.clear();
  }

  size_t prefetch(const K &k) { return 1; }
  size_t size() const { return _size; }
  uint64_t memuse() const {
    return sizeof(K) * capacity + sizeof(V) * capacity +
           sizeof(bool) * capacity;
  }

private:
  int capacity = 0;
  int _size = 0;
  int effective_size = 0;
  std::vector<K> keys;
  std::vector<V> values;
  std::vector<bool> occupied;
};

} // namespace crash

#endif
