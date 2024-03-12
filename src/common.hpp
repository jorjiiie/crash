#pragma once

#include <concepts>
#ifndef COMMON_HPP
#define COMMON_HPP

#include <atomic>
#include <cstring>
#include <functional>
#include <random>
#include <string>

namespace crash {
template <class Key>
concept Hashable = requires(const Key &k) {
  { k.hash() } -> std::convertible_to<std::size_t>;
  { k <=> k } -> std::convertible_to<int>;
  { k == k } -> std::convertible_to<bool>;
};

struct pcg32_random_t {
  pcg32_random_t(uint64_t a, uint64_t b) : state(a), inc(b) {}
  uint64_t state;
  uint64_t inc;
};

static inline uint32_t pcg32_random_r(pcg32_random_t *rng) {
  uint64_t oldstate = rng->state;
  // Advance internal state
  rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
  // Calculate output function (XSH RR), uses old state for max ILP
  uint32_t xorshifted = ((oldstate >> 18u) ^ oldstate) >> 27u;
  uint32_t rot = oldstate >> 59u;
  return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

class pcg32 {
public:
  pcg32(uint64_t a, uint64_t b) : state(a, b) {}
  uint32_t get() { return pcg32_random_r(&state); }
  pcg32_random_t state;
};
template <class K, class Hash>
concept HashFn = requires(const K &k) {
  { Hash{}(k) } -> std::convertible_to<std::size_t>;
};

template <class T> class Atomic : public std::atomic<T> {
public:
  Atomic() = default;
  constexpr Atomic(T desired) : std::atomic<T>(desired) {}
  constexpr Atomic(const Atomic<T> &other)
      : Atomic(other.load(std::memory_order_acquire)) {}
  Atomic &operator=(const Atomic<T> &other) {
    this->store(other.load(std::memory_order_relaxed),
                std::memory_order_release);
    return *this;
  }
};

template <typename hash_fn> struct string_wrapper {
  string_wrapper() { std::memset(s, 0, 32); }
  string_wrapper(const std::string &x) {
    std::memset(s, 0, 32);
    for (int i = 0; i < x.length(); i++) {
      s[i] = x[i];
    }
    s[x.length()] = 0;
  }
  string_wrapper(const char *str) {
    std::memset(s, 0, 32);
    std::strcpy(s, str);
  }
  string_wrapper(const string_wrapper &w) { std::strcpy(s, w.s); }

  char s[32] = {};
  int operator<=>(const string_wrapper &o) const {
    for (int i = 0; i < 31; i++) {
      if (o.s[i] != s[i])
        return -1;
    }
    return 0;
  }
  template <class T> bool operator==(const string_wrapper<T> &other) const {
    return (*this) <=> other == 0;
  }
  size_t hash() const { return hash_fn{}(*this); }
};

struct _hash {
  size_t operator()(const string_wrapper<_hash> &s) const {
    unsigned long hash = 5381;
    int c;

    const char *str = s.s;

    while ((c = *str++))
      hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
  }
};
inline uint64_t squirrel3(uint64_t at) {
  constexpr uint64_t BIT_NOISE1 = 0x9E3779B185EBCA87ULL;
  constexpr uint64_t BIT_NOISE2 = 0xC2B2AE3D27D4EB4FULL;
  constexpr uint64_t BIT_NOISE3 = 0x27D4EB2F165667C5ULL;
  at *= BIT_NOISE1;
  at ^= (at >> 8);
  at += BIT_NOISE2;
  at ^= (at << 8);
  at *= BIT_NOISE3;
  at ^= (at >> 8);
  return at;
}
template <typename hash_fn> struct uint64_t_wrapper {
  uint64_t_wrapper() {}
  uint64_t_wrapper(uint64_t t) : i(t) {}
  uint64_t_wrapper(int t) : i(t) {}

  size_t hash() const { return hash_fn{}(i); }
  uint64_t operator<=>(const uint64_t_wrapper &o) const { return o.i - i; }
  bool operator==(const uint64_t_wrapper &o) const { return o.i == i; }

  uint64_t i;
};
struct ihash {
  size_t operator()(const uint64_t_wrapper<ihash> &i) const {
    return std::hash<uint64_t>{}(i.i);
  }
};
struct sqhash {
  size_t operator()(const uint64_t_wrapper<sqhash> &i) const {
    return squirrel3(i.i);
  }
};

typedef string_wrapper<_hash> String;
typedef uint64_t_wrapper<ihash> i64_std;
typedef uint64_t_wrapper<sqhash> i64;

std::ostream &operator<<(std::ostream &os, const String &s) {
  os << s.s;
  return os;
}
std::ostream &operator<<(std::ostream &os, i64 i) {
  os << i.i;
  return os;
}
struct gen_string {
  static std::string generate(int max_length) {
    static std::string possible_characters =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    static std::random_device rd;
    static std::mt19937 engine(rd());
    static std::uniform_int_distribution<> dist(0,
                                                possible_characters.size() - 1);
    std::string ret = "";
    for (int i = 0; i < max_length; i++) {
      int random_index =
          dist(engine); // get index between 0 and possible_characters.size()-1
      ret += possible_characters[random_index];
    }
    return ret;
  }

  String get() const { return String(generate(30)); }
};

struct gen_int {
  gen_int() : rng(1, 10) {}
  pcg32 rng;
  i64 get() { return (uint64_t)rng.get() * rng.get(); }
};
struct gen_int_std : public gen_int {
  i64_std get() { return (uint64_t)rng.get() * rng.get(); }
};
struct gen_int_unwrap {
  gen_int_unwrap() : rng(1, 10) {}
  pcg32 rng;
  uint64_t get() { return (uint64_t)rng.get() * rng.get(); }
};

} // namespace crash

#endif
