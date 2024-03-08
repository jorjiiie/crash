#include <array>
#include <cassert>
#include <chrono>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "common.hpp"
#include "crash_multi.hpp"
#include "quadratic.hpp"

using namespace std;
using namespace crash;

template <class T> inline void doNotOptimizeAway(T &&datum) {
  asm volatile("" : "+r"(datum));
}
template <class M, class K, class V>
concept Hashtable = requires(M m, const K &k, V v) {
  { m.get(k) } -> same_as<optional<V>>;
  { m.find(k) } -> same_as<V>;
  { m.put(k, v) } -> same_as<void>;
  { m.erase(k) } -> same_as<void>;
  { m.prefetch(k) } -> same_as<size_t>;
  { m.clear() } -> same_as<void>;
  { m.memuse() } -> same_as<uint64_t>; // in bytes
  { m.size() } -> same_as<size_t>;
};

template <class G, class T>
concept Generator = requires(G g) {
  { g.get() } -> same_as<T>;
};

template <class K, class V>
  requires Hashable<K>
class Std_Unordered {
public:
  optional<V> get(const K &k) const {
    if (auto it = mp.find(k); it != mp.end())
      return it->second;
    return {};
  }
  V find(const K &k) { return mp[k]; }
  void put(const K &k, V v) { mp[k] = v; }
  void erase(const K &k) { mp.erase(k); }
  size_t prefetch(const K &k) { return 0; }
  void clear() { mp.clear(); }
  uint64_t memuse() {
    return 0; // id ont know lol
  }
  size_t size() { return mp.size(); }

private:
  struct hasher {
    size_t operator()(const K &k) const { return k.hash(); }
  };
  unordered_map<K, V, hasher> mp;
};
class Clock {
public:
  Clock(std::function<void(uint64_t)> cbk) : callback(cbk) {
    start = Clock_::now();
  }
  ~Clock() {
    auto end = Clock_::now();
    auto n = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                 .count();
    callback(n);
  }

private:
  typedef std::chrono::high_resolution_clock Clock_;
  std::function<void(uint64_t)> callback;

  std::chrono::time_point<std::chrono::high_resolution_clock> start;
};

template <class M, class K, class V, class G1, class G2, size_t N>
  requires Hashable<K> && Hashtable<M, K, V> && Generator<G1, K> &&
           Generator<G2, V>
unordered_map<string, uint64_t> bench() {
  M m;
  unordered_map<string, uint64_t> results;

  // probably buffer these...
  G1 keygen_;
  G2 valgen_;

  pcg32 rng(rand(), rand());

  unordered_set<K, decltype([](const auto &z) { return z.hash(); })> ks;
  vector<K> keys(2 * N);
  vector<V> vals(2 * N);

  while (ks.size() != 2 * N) {
    ks.insert(keygen_.get());
  }

  {
    int i = 0;
    for (const auto &k : ks) {
      keys[i++] = k;
    }
  }

  for (int i = 0; i < 2 * N; i++) {
    vals[i] = valgen_.get();
  }
  uint64_t _ = 0;

  auto make_clock = [&results](string n) {
    return Clock([&results, n](uint64_t ns) { results[n] = ns; });
  };

  // can't "fix" the load factor here
  {
    auto c = make_clock("insert_N");
    for (int i = 0; i < N; i++) {
      m.put(keys[i], vals[i]);
    }
  }

  const int num_bench = 1000000;
  {
    auto c = make_clock("get_N_present");
    int idx = 0;
    for (int i = 0; i < num_bench; i++) {
      auto x = m.get(keys[idx]);
      doNotOptimizeAway(*x);
      idx++;
      if (idx == N)
        idx = 0;
    }
  }
  {
    auto c = make_clock("find_N_present");
    int idx = 0;
    for (int i = 0; i < num_bench; i++) {
      auto x = m.find(keys[idx]);
      doNotOptimizeAway(x);
      idx++;
      if (idx == N)
        idx = 0;
    }
  }
  {
    auto c = make_clock("get_N_present_random");
    for (int i = 0; i < num_bench; i++) {
      int z = rng.get() % N;   // the modulo is slow
      auto x = m.get(keys[z]); // cache miss here
      doNotOptimizeAway(*x);
    }
  }
  {
    auto c = make_clock("get_N_missing");
    int idx = 0;
    for (int i = 0; i < num_bench; i++) {
      auto x = m.get(keys[N + idx++]);
      doNotOptimizeAway(*x);
      if (idx == N)
        idx = 0;
    }
  }
  { auto c = make_clock("get_N_missing_random"); }
  { auto c = make_clock("get_N_mixed_50"); }
  { auto c = make_clock("erase_N_present"); } // hmm how does this work...
  { auto c = make_clock("erase_N_mixed_50"); }
  { auto c = make_clock("use_N_flushed_cache"); } // mixed workload
  {
    auto c = make_clock("use_N_prefetch");
    // unrolls and prefetches
  }
  {
    auto c = make_clock("clear");
    m.clear();
  }
  { // probe length
  }
  return results;
}

template <class M, class K, class V, class G1, class G2, size_t N>
  requires Hashable<K> && Hashtable<M, K, V> && Generator<G1, K> &&
           Generator<G2, V>
void do_bench(string n, ostream &stream) {
  stream << "BEGIN " << n << "\n------------------\n";
  auto z = bench<M, K, V, G1, G2, N>();
  for (const auto &[name, val] : z) {
    std::cout << name << ": " << val << " ns\n";
  }
  stream << "END " << n << "\n------------------\n";
}

int main() {
  const int string_inserts = 10000000;
  map<string, function<void(string, ostream &)>> benchmarks = {
      {"Std Unordered",
       do_bench<Std_Unordered<String, uint64_t>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Quadratic Probing",
       do_bench<quadratic<String, uint64_t>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
  };

  for (const auto &[n, fn] : benchmarks) {
    fn(n, cerr);
  }
}
