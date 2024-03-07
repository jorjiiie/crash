#include "crash.hpp"
#include "crash_multi.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <thread>

#include <set>
using namespace std;
using namespace crash;

template <typename hash_fn> struct string_wrapper {
  string_wrapper() { memset(s, 0, 32); }
  string_wrapper(const string &x) {
    memset(s, 0, 32);
    for (int i = 0; i < x.length(); i++) {
      s[i] = x[i];
    }
    s[x.length()] = 0;
  }
  string_wrapper(const char *str) {
    memset(s, 0, 32);
    strcpy(s, str);
  }
  string_wrapper(const string_wrapper &w) { strcpy(s, w.s); }

  char s[32] = {};
  int operator<=>(const string_wrapper &o) const {
    for (int i = 0; i < 31; i++) {
      if (o.s[i] != s[i])
        return -1;
    }
    return 0;
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

ostream &operator<<(ostream &os, const string_wrapper<_hash> &s) {
  os << s.s;
  return os;
}

string generate(int max_length) {
  static string possible_characters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  static random_device rd;
  static mt19937 engine(rd());
  static uniform_int_distribution<> dist(0, possible_characters.size() - 1);
  string ret = "";
  for (int i = 0; i < max_length; i++) {
    int random_index =
        dist(engine); // get index between 0 and possible_characters.size()-1
    ret += possible_characters[random_index];
  }
  return ret;
}

int main() {
  using str = string_wrapper<_hash>;
  vector<str> keys;
#define NUM_KEYS 20000000
#define NUM_OPS 15000000

  cerr << " wtf\n";

  for (int i = 0; i < NUM_KEYS; i++) {
    keys.push_back(str(generate(30)));
  }
  cerr << "cool\n";
  typedef std::chrono::high_resolution_clock Clock;

  for (int i = 0; i < 50; i++) {
    auto t0 = Clock::now();

    pcg32 rng(i, i);
    hashtable<string_wrapper<_hash>, int> x(1 << 25);
    for (int i = 0; i < NUM_OPS; i++) {
      // get an op
      x.put(keys[i], rng.get());
    }
    auto t1 = Clock::now();
    std::chrono::duration<float> t = t1 - t0;
    cout << "time to insert 15m: " << t.count() << "s\n";

    t0 = Clock::now();
    int cnt = 0;
    for (int i = 0; i < NUM_OPS; i++) {
      int r = rng.get() % NUM_KEYS;
      auto z = x.get(keys[r]);
      if (z) {
        cnt++;
      }
    }
    t1 = Clock::now();
    t = t1 - t0;
    cout << t.count() << "s please " << cnt << "\n";
  }

  exit(0);
  auto t0 = Clock::now();
  {
    concurrent_hashtable<string_wrapper<_hash>, int> x(1 << 25);

    vector<thread> threads;
#define NUM_THREADS 6
    for (int i = 0; i < NUM_THREADS; i++) {
      threads.push_back(thread([&x, i, &keys]() {
        thread_local pcg32 rng(i, i);
        for (int j = 0; j < 5; j++)
          rng.get();

        for (int j = 0; j < NUM_OPS / NUM_THREADS; j++) {
          // get an op
          double op = (rng.get() * 0.5 / RAND_MAX);
          int r = rng.get() % NUM_KEYS;

          if (op < 0.8) {
            x.get(keys[r]);
          } else if (op < 0.95) {
            // zz.insert(keys[r].s);
            x.put(keys[r], 0);
          } else {
            x.erase(keys[r]);
            // zz.erase(keys[r].s);00000
          }
        }
      }));
    }
    for (auto &z : threads) {
      z.join();
    }
  }
  auto t1 = Clock::now();
  std::chrono::duration<float> t = t1 - t0;
  cout << t.count() << "s\n";

  t0 = Clock::now();
  {
    concurrent_hashtable<string_wrapper<_hash>, int> x(1 << 25);

    vector<thread> threads;
    threads.push_back(thread([&x, &keys]() {
      pcg32 rng(0, 0);
      for (int j = 0; j < 5; j++)
        rng.get();

      for (int j = 0; j < NUM_OPS; j++) {
        // get an op
        double op = (rng.get() * 0.5 / RAND_MAX);
        int r = rng.get() % NUM_KEYS;

        if (op < 0.8) {
          x.get(keys[r]);
        } else if (op < 0.95) {
          // zz.insert(keys[r].s);
          x.put(keys[r], 0);
        } else {
          x.erase(keys[r]);
          // zz.erase(keys[r].s);
        }
      }
    }));
    for (auto &z : threads) {
      z.join();
    }
  }
  t1 = Clock::now();
  t = t1 - t0;
  cout << t.count() << "s\n";

  // x.dump();
}
