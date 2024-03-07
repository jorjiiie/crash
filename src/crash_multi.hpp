#pragma once

#ifndef CRASH_MULTI_HPP
#define CRASH_MULTI_HPP

#include <atomic>
#include <functional>
#include <optional>
#include <vector>

#include <iostream>

#include "common.hpp"

namespace crash {

template <class K, class V> struct alignas(64) kv_entry {
  kv_entry() { memset(this, 0, sizeof(*this)); }
  K key;
  struct state {
    int occupied : 1;
    int tombstone : 1;
    V value;
  };
  Atomic<state> s;
};

template <class Key, class Value>
  requires Hashable<Key>
class concurrent_hashtable {
public:
  using K = Key;
  using V = Value;
  using table_entry = kv_entry<K, V>;

  concurrent_hashtable(size_t size = 16) : table(size), current_size(size){};
  ~concurrent_hashtable(){};

  std::optional<const std::reference_wrapper<const V>> get(const K &key) const {
    // use quadratic probing
    auto &entry = get_entry(key);

    auto s = entry.s.load();
    if (s.occupied) {
      return s.value;
    }
    return {};
  }

  void put(const K &key, V v) {
    auto &entry = get_entry(key);
    auto s = entry.s.load();
    if (s.occupied) {
      auto n_s = s;
      s.value = v;
      if (entry.s.compare_exchange_strong(s, n_s)) {
        return;
      }
      // dont know what to do lol
      put(key, v);
      return;
    }

    // insert
    num_keys++;
    effective_keys++;
    auto n_s = s;
    n_s.tombstone = false;
    n_s.occupied = true;
    n_s.value = v;
    if (!entry.s.compare_exchange_strong(s, n_s)) {
      put(key, v);
      return;
    }

    // this is uhhh not great
    // lets just... not do this for now!
    // need to swap tables basically
    /*
    if (effective_keys * 2 > current_size) {
      concurrent_hashtable x(current_size * 2);
      int p = 0;
      for (int i = 0; i < current_size; i++) {
        p++;
        if (occupied[i]) {
          x.put(table[i].first, table[i].second);
        }
        x.put(key, v);
      }
      std::swap(*this, x);
    }
    */
  }

  bool erase(const K &key) {

    auto &entry = get_entry(key);
    auto s = entry.s.load();

    if (s.occupied) {
      num_keys--;
      effective_keys--;
      auto n_s = s;
      n_s.tombstone = true;
      n_s.occupied = false;
      if (entry.s.compare_exchange_strong(s, n_s)) {
        return true;
      }
      return erase(key);
    }

    return false;
  }
  size_t size() const { return num_keys; }
  size_t capacity() const { return current_size; }

  void dump() const {
    for (int i = 0; i < current_size; i++) {

      auto &entry = table[i];
      auto s = entry.s.load();
      if (s.occupied) {
        std::cerr << i << ": " << entry.key << " " << s.value << "\n";
      }
    }
  }

private:
  [[nodiscard]] inline size_t
  get_slot(const K &key) const { // return a reference to the entry?
    size_t h = key.hash() % table.size();
    int i = 1;
    auto entry = table[h];
    auto s = entry.s.load();
    while ((s.tombstone || s.occupied) && entry.key <=> key != 0) {
      h += i;
      i++;
      h &= (current_size - 1);

      entry = table[h];
      s = entry.s.load();
    }
    return h;
  }
  [[nodiscard]] inline const table_entry &get_entry(const K &key) const {
    size_t h = key.hash() % table.size();
    int i = 1;
    auto entry = table[h];
    auto s = entry.s.load();
    std::cerr << std::boolalpha;
    while ((s.tombstone || s.occupied) && entry.key <=> key != 0) {

      h += i;
      i++;
      h &= (current_size - 1);

      entry = table[h];
      s = entry.s.load();
    }
    return table[h];
  }
  // either returns the right kv_pair, or just returns an empty cell
  [[nodiscard]] inline table_entry &get_entry(const K &key) {
    size_t h = key.hash() % table.size();
    int i = 1;
    auto entry = table[h];
    auto s = entry.s.load();
    std::cerr << std::boolalpha;
    while ((s.tombstone || s.occupied) && entry.key <=> key != 0) {

      h += i;
      i++;
      h &= (current_size - 1);

      entry = table[h];
      s = entry.s.load();
    }
    return table[h];
  }
  std::atomic<size_t> num_keys = 0;
  std::atomic<size_t> effective_keys = 0;
  std::atomic<size_t> current_size = 16;
  std::vector<table_entry> table;
};

} // namespace crash

#endif
