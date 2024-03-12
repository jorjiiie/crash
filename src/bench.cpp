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
#include "linear.hpp"
#include "quadratic.hpp"
#include "robinhood.hpp"

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
map<string, uint64_t> bench() {
  M m;
  map<string, uint64_t> results;

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
  const int num_erase = 1000;
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
  {
    auto c = make_clock("get_N_missing_random");
    for (int i = 0; i < num_bench; i++) {
      int z = rng.get() % N;
      auto x = m.get(keys[N + z]);
      doNotOptimizeAway(*x);
    }
  }
  {
    auto c = make_clock("get_N_mixed_50");
    for (int i = 0; i < num_bench; i++) {
      int z = rng.get() % (2 * N);
      auto x = m.get(keys[z]);
      doNotOptimizeAway(*x);
    }
  }

  // get erase indices

  const int erase_task = 10000;
  {
    auto tbl = m;
    {
      auto c = make_clock("erase_N_present");
      for (int i = 0; i < erase_task; i++) {
        tbl.erase(keys[i]);
      }
    }
  }
  {
    auto tbl = m;
    {
      auto c = make_clock("erase_N_mixed_50");
      for (int i = 0; i < erase_task; i++) {
        int z = rng.get() % (2 * N);
        tbl.erase(keys[z]);
      }
    }
  }

  // uhhh this may be useless
  {
    auto c = make_clock("clear");
    m.clear();
  }
  return results;
}

template <class M, class K, class V, class G1, class G2, size_t N>
  requires Hashable<K> && Hashtable<M, K, V> && Generator<G1, K> &&
           Generator<G2, V>
void do_bench(string n, ostream &stream) {
  cerr << "BEGIN " << n << "\n------------------\n";
  stream << n;
  auto z = bench<M, K, V, G1, G2, N>();
  for (const auto &[name, val] : z) {
    stream << ", " << val;
  }
  stream << "\n";
  cerr << "\n------------------\n"
       << "END " << n << "\n";
}

int main() {
  const int string_inserts = 10000000;
  const int int_inserts = 10000000;
  map<string, function<void(string, ostream &)>> benchmarks = {
      // unordered_map
      {"Std Unordered String",
       do_bench<Std_Unordered<String, uint64_t>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Std Unordered Int Std",
       do_bench<Std_Unordered<i64_std, uint64_t>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},

      // quadratic
      {"Quadratic 50 String",
       do_bench<quadratic<String, uint64_t, 50>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Quadratic 50 Int Std",
       do_bench<quadratic<i64_std, uint64_t, 50>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},
      {"Quadratic 70 String",
       do_bench<quadratic<String, uint64_t, 70>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Quadratic 70 Int Std",
       do_bench<quadratic<i64_std, uint64_t, 70>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},
      {"Quadratic 90 String",
       do_bench<quadratic<String, uint64_t, 90>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Quadratic 90 Int Std",
       do_bench<quadratic<i64_std, uint64_t, 90>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},

      // linear
      {"Linear 50 String",
       do_bench<linear<String, uint64_t, 50>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Linear 50 Int Std",
       do_bench<linear<i64_std, uint64_t, 50>, i64_std, uint64_t, gen_int_std,
                gen_int_unwrap, int_inserts>},
      {"Linear 70 String",
       do_bench<linear<String, uint64_t, 70>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Linear 70 Int Std",
       do_bench<linear<i64_std, uint64_t, 70>, i64_std, uint64_t, gen_int_std,
                gen_int_unwrap, int_inserts>},
      {"Linear 90 String",
       do_bench<linear<String, uint64_t, 90>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Linear 90 Int Std",
       do_bench<linear<i64_std, uint64_t, 90>, i64_std, uint64_t, gen_int_std,
                gen_int_unwrap, int_inserts>},

      // robinhood
      {"Robinhood 50 Int Std",
       do_bench<robinhood<i64_std, uint64_t, 50>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},
      {"Robinhood 50 String",
       do_bench<robinhood<String, uint64_t, 50>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Ronibhood 70 Int Std",
       do_bench<robinhood<i64_std, uint64_t, 70>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},
      {"Robinhood 70 String",
       do_bench<robinhood<String, uint64_t, 70>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},
      {"Robinhood 90 Int Std",
       do_bench<robinhood<i64_std, uint64_t, 90>, i64_std, uint64_t,
                gen_int_std, gen_int_unwrap, int_inserts>},
      {"Robinhood 90 String",
       do_bench<robinhood<String, uint64_t, 90>, String, uint64_t, gen_string,
                gen_int_unwrap, string_inserts>},

  };

  freopen("out.csv", "w", stdout);
  cout << "name";
  for (const auto &[n, _] : bench<Std_Unordered<String, uint64_t>, String,
                                  uint64_t, gen_string, gen_int_unwrap, 1>()) {

    cout << ", " << n;
  }
  cout << "\n";

  gen_int_std x;
  for (int i = 0; i < 50; i++) {
    auto z = x.get();
    cerr << "bruh: " << z.i << " " << z.hash() << "\n";
  }

  for (const auto &[n, fn] : benchmarks) {
    fn(n, cout);
  }
}
