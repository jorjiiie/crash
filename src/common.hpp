#pragma once

#ifndef COMMON_HPP
#define COMMON_HPP

#include <atomic>
#include <functional>

namespace crash {
template <class Key>
concept Hashable = requires(const Key &k) {
  { k.hash() } -> std::convertible_to<std::size_t>;
  { k <=> k } -> std::convertible_to<int>;
};

typedef struct {
  uint64_t state;
  uint64_t inc;
} pcg32_random_t;

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
} // namespace crash

#endif
