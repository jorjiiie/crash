#pragma once

#ifndef CRASH_HPP
#define CRASH_HPP

#include <functional>
#include <optional>
#include <vector>

#include <iostream>
#include <set>
#include <string>

namespace crash {

template <class Key>
concept Hashable = requires(const Key &k) {
  { k.hash() } -> std::convertible_to<std::size_t>;
  { k <=> k } -> std::convertible_to<int>;
};

template <class Key, class Value>
  requires Hashable<Key>
class hashtable {
public:
  using K = Key;
  using V = Value;

  hashtable(size_t size = 16)
      : table(size), tombstones(size), occupied(size), current_size(size){};
  ~hashtable(){};

  std::optional<const std::reference_wrapper<V>> get(const K &key) {
    // use quadratic probing
    size_t h = get_slot(key);
    // can save a comparison here
    if (occupied[h] && table[h].first <=> key == 0)
      return table[h].second;
    return {};
  }
  std::optional<const std::reference_wrapper<const V>> get(const K &key) const {
    // use quadratic probing
    size_t h = get_slot(key);
    // can save a comparison here
    if (occupied[h] && table[h].first <=> key == 0)
      return table[h].second;
    return {};
  }

  void put(const K &key, V v) {
    size_t h = get_slot(key);
    // std::cerr << "putting " << key << " in " << h << " " << num_keys << "\n";

    if (occupied[h]) {
      table[h].second = v;
      return;
    }
    // insert
    num_keys++;
    effective_keys++;
    occupied[h] = true;
    tombstones[h] = false;
    table[h] = {key, v};

    if (effective_keys * 2 > current_size) {
      hashtable x(current_size * 2);
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
  }

  bool erase(const K &key) {
    size_t h = get_slot(key);
    if (occupied[h] && table[h].first <=> key == 0) {
      num_keys--;
      tombstones[h] = true;
      occupied[h] = false;
      return true;
    }
    return false;
  }
  size_t size() const { return num_keys; }
  size_t capacity() const { return current_size; }

  void dump() const {
    std::set<std::string> zz;
    int cnt = 0;
    for (int i = 0; i < current_size; i++) {

      if (occupied[i]) {
        cnt++;
        zz.insert(table[i].first.s);
        std::cerr << i << ": " << table[i].first << " " << table[i].second
                  << "\n";
      }
    }
    for (auto s : zz) {
      std::cerr << s << "\n";
    }
  }

private:
  [[nodiscard]] inline size_t get_slot(const K &key) const {
    size_t h = key.hash() % table.size();
    int i = 1;
    while ((tombstones[h] || occupied[h]) && table[h].first <=> key != 0) {
      h += i * (i + 1) / 2;
      i++;
      h &= (current_size - 1);
    }
    // std::cerr << "I return " << h << " for " << key << "\n";
    return h;
  }
  size_t num_keys = 0;
  size_t effective_keys = 0;
  size_t current_size = 16;
  std::vector<std::pair<K, V>> table;
  std::vector<bool> tombstones;
  std::vector<bool> occupied;
};

} // namespace crash

#endif
