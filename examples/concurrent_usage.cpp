#include "pxhash/concurrent_pxhash.hpp"

#include <cassert>
#include <thread>
#include <vector>

int main() {
  pxhash::ConcurrentPXHash<int, int> map(16);
  std::vector<std::thread> threads;

  for (int thread_id = 0; thread_id < 4; ++thread_id) {
    threads.emplace_back([thread_id, &map] {
      for (int i = 0; i < 100; ++i) {
        const int key = thread_id * 1000 + i;
        map.insert(key, key * 2);
      }
    });
  }

  for (auto& thread : threads) thread.join();

  int value = 0;
  assert(map.find(2001, value));
  assert(value == 4002);
}
