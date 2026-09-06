#include <cassert>
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "pxhash/concurrent_pxhash.hpp"
#include "pxhash/pxhash.hpp"

namespace {

struct ConstantHash {
  std::size_t operator()(std::uint64_t) const noexcept { return 0; }
};

struct NoDefaultValue {
  explicit NoDefaultValue(int v) : value(v) {}
  int value;
};

struct TransparentStringHash {
  using is_transparent = void;

  std::size_t operator()(std::string_view value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }

  std::size_t operator()(const std::string& value) const noexcept {
    return (*this)(std::string_view(value));
  }
};

struct TransparentStringEqual {
  using is_transparent = void;

  bool operator()(std::string_view lhs, std::string_view rhs) const noexcept {
    return lhs == rhs;
  }
};

template <class T>
struct CountingAllocator {
  using value_type = T;

  CountingAllocator() = default;

  template <class U>
  CountingAllocator(const CountingAllocator<U>&) {}

  T* allocate(std::size_t n) {
    return std::allocator<T>{}.allocate(n);
  }

  void deallocate(T* p, std::size_t n) noexcept {
    std::allocator<T>{}.deallocate(p, n);
  }
};

template <class T, class U>
bool operator==(const CountingAllocator<T>&, const CountingAllocator<U>&) {
  return true;
}

template <class T, class U>
bool operator!=(const CountingAllocator<T>&, const CountingAllocator<U>&) {
  return false;
}

void test_insert_find_and_update() {
  pxhash::PXHash<std::string, int> map;

  assert(map.empty());

  assert(map.insert("alpha", 1));
  assert(map.insert("beta", 2));
  assert(!map.insert("alpha", 3));

  int value = 0;
  assert(map.size() == 2);
  assert(map.capacity() >= map.size());
  assert(map.load_factor() > 0.0F);
  assert(map.contains("alpha"));
  assert(map.find("alpha", value));
  assert(value == 3);
  assert(map.find("alpha") != nullptr);
  assert(*map.find("alpha") == 3);
  assert(map.find("beta", value));
  assert(value == 2);
  assert(!map.find("gamma", value));
}

void test_iterators_visit_entries() {
  pxhash::PXHash<int, int> map;
  map.insert(1, 10);
  map.insert(2, 20);
  map.insert(3, 30);

  int key_sum = 0;
  int value_sum = 0;
  for (const auto& entry : map) {
    key_sum += entry.key;
    value_sum += entry.value;
  }

  assert(key_sum == 6);
  assert(value_sum == 60);
  assert(map.begin() != map.end());
}

void test_heterogeneous_string_lookup() {
  pxhash::PXHash<std::string, int, TransparentStringHash, TransparentStringEqual> map;
  map.insert("alpha", 10);

  std::string_view lookup = "alpha";
  assert(map.contains(lookup));
  assert(map.find(lookup) != nullptr);
  assert(*map.find(lookup) == 10);
}

void test_custom_allocator_instantiation() {
  using Slot = pxhash::Slot<int, int>;
  pxhash::PXHash<int, int, std::hash<int>, std::equal_to<int>, CountingAllocator<Slot>> map;

  map.insert(1, 2);
  assert(map.contains(1));
}

void test_operator_brackets_and_clear() {
  pxhash::PXHash<std::string, int> map;
  map["alpha"] = 5;
  ++map["alpha"];

  assert(map.size() == 1);
  assert(*map.find("alpha") == 6);

  map.clear();
  assert(map.empty());
  assert(!map.contains("alpha"));
}

void test_try_emplace_supports_non_default_constructible_values() {
  pxhash::PXHash<int, NoDefaultValue> map;

  assert(map.try_emplace(1, 10));
  assert(!map.try_emplace(1, 20));

  NoDefaultValue* value = map.find(1);
  assert(value != nullptr);
  assert(value->value == 10);
}

void test_tail_wraparound_collision_survives_rehash() {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map;

  map.insert(31, 310);
  map.insert(63, 630);
  assert(map.size() == 2);

  std::uint64_t value = 0;
  assert(map.find(31, value));
  assert(value == 310);
  assert(map.find(63, value));
  assert(value == 630);

  map.reserve(1000);
  assert(map.size() == 2);
  assert(map.find(31, value));
  assert(value == 310);
  assert(map.find(63, value));
  assert(value == 630);
}

void test_erase_and_reuse_deleted_slots() {
  pxhash::PXHash<int, int> map;

  for (int i = 0; i < 32; ++i) {
    map.insert(i, i * 10);
  }

  for (int i = 0; i < 16; ++i) {
    assert(map.erase(i));
  }

  assert(map.size() == 16);
  assert(!map.erase(999));

  for (int i = 100; i < 116; ++i) {
    map.insert(i, i * 10);
  }

  assert(map.size() == 32);

  int value = 0;
  for (int i = 0; i < 16; ++i) {
    assert(!map.find(i, value));
  }
  for (int i = 16; i < 32; ++i) {
    assert(map.find(i, value));
    assert(value == i * 10);
  }
  for (int i = 100; i < 116; ++i) {
    assert(map.find(i, value));
    assert(value == i * 10);
  }
}

void test_growth_preserves_values() {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map;

  for (std::uint64_t i = 0; i < 512; ++i) {
    map.insert(i, i + 1000);
  }

  assert(map.size() == 512);

  std::uint64_t value = 0;
  for (std::uint64_t i = 0; i < 512; ++i) {
    assert(map.find(i, value));
    assert(value == i + 1000);
  }
}

void test_collision_heavy_workload() {
  pxhash::PXHash<std::uint64_t, std::uint64_t, ConstantHash> map;

  for (std::uint64_t i = 0; i < 96; ++i) {
    map.insert(i, i * 2);
  }

  std::uint64_t value = 0;
  for (std::uint64_t i = 0; i < 96; ++i) {
    assert(map.find(i, value));
    assert(value == i * 2);
  }

  for (std::uint64_t i = 0; i < 48; ++i) {
    assert(map.erase(i));
  }
  for (std::uint64_t i = 96; i < 144; ++i) {
    map.insert(i, i * 2);
  }

  assert(map.size() == 96);

  for (std::uint64_t i = 0; i < 48; ++i) {
    assert(!map.find(i, value));
  }
  for (std::uint64_t i = 48; i < 144; ++i) {
    assert(map.find(i, value));
    assert(value == i * 2);
  }
}

void test_move_insert_support() {
  pxhash::PXHash<std::string, std::string> map;
  std::string key = "k";
  std::string value = "payload";

  map.insert(std::move(key), std::move(value));

  std::string out;
  assert(map.find("k", out));
  assert(out == "payload");
}

void test_copy_support() {
  pxhash::PXHash<std::string, int> original;
  original.insert("alpha", 1);
  original.insert("beta", 2);

  pxhash::PXHash<std::string, int> copy = original;
  assert(copy.size() == 2);
  assert(*copy.find("alpha") == 1);
  assert(*copy.find("beta") == 2);
}

void test_randomized_against_unordered_map() {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map;
  std::unordered_map<std::uint64_t, std::uint64_t> reference;
  std::mt19937_64 rng(123456);

  for (int step = 0; step < 5000; ++step) {
    const std::uint64_t key = rng() % 512;
    const std::uint64_t value = rng();
    const int op = static_cast<int>(rng() % 3);

    if (op == 0) {
      const bool inserted = reference.find(key) == reference.end();
      reference[key] = value;
      assert(map.insert_or_assign(key, value) == inserted);
    } else if (op == 1) {
      const bool erased = reference.erase(key) != 0;
      assert(map.erase(key) == erased);
    } else {
      const auto it = reference.find(key);
      std::uint64_t found_value = 0;
      assert(map.find(key, found_value) == (it != reference.end()));
      if (it != reference.end()) assert(found_value == it->second);
    }

    assert(map.size() == reference.size());
  }

  for (const auto& [key, value] : reference) {
    std::uint64_t found_value = 0;
    assert(map.find(key, found_value));
    assert(found_value == value);
  }
}

void test_concurrent_wrapper() {
  pxhash::ConcurrentPXHash<int, int> map(8);
  std::vector<std::thread> threads;

  for (int thread_id = 0; thread_id < 4; ++thread_id) {
    threads.emplace_back([thread_id, &map] {
      for (int i = 0; i < 250; ++i) {
        const int key = thread_id * 1000 + i;
        map.insert(key, key * 2);
      }
    });
  }

  for (auto& thread : threads) thread.join();

  assert(map.size() == 1000);
  int value = 0;
  assert(map.find(1001, value));
  assert(value == 2002);
  assert(map.erase(1001));
  assert(!map.contains(1001));
}

void test_binary_roundtrip_for_trivial_types() {
  const char* path = "pxhash_roundtrip.bin";

  pxhash::PXHash<std::uint64_t, std::uint64_t> original;
  for (std::uint64_t i = 0; i < 128; ++i) {
    original.insert(i + 10, i * 100);
  }

  assert(original.saveBinary(path));

  pxhash::PXHash<std::uint64_t, std::uint64_t> restored;
  restored.insert(999, 111);
  assert(restored.loadBinary(path));
  assert(restored.size() == original.size());

  std::uint64_t value = 0;
  for (std::uint64_t i = 0; i < 128; ++i) {
    assert(restored.find(i + 10, value));
    assert(value == i * 100);
  }
  assert(!restored.find(999, value));

  std::remove(path);
}

void test_binary_load_rejects_corrupt_or_trailing_data() {
  {
    std::ofstream out("pxhash_corrupt.bin", std::ios::binary | std::ios::trunc);
    out << "bad";
  }

  pxhash::PXHash<std::uint64_t, std::uint64_t> map;
  assert(!map.loadBinary("pxhash_corrupt.bin"));
  std::remove("pxhash_corrupt.bin");

  pxhash::PXHash<std::uint64_t, std::uint64_t> original;
  original.insert(1, 2);
  assert(original.save_binary("pxhash_trailing.bin"));

  {
    std::ofstream out("pxhash_trailing.bin", std::ios::binary | std::ios::app);
    out << "x";
  }

  assert(!map.load_binary("pxhash_trailing.bin"));
  std::remove("pxhash_trailing.bin");
}

void test_binary_serialization_rejects_non_trivial_types() {
  pxhash::PXHash<std::string, std::string> map;
  map.insert("alpha", "beta");

  assert(!map.saveBinary("pxhash_strings.bin"));
  assert(!map.loadBinary("pxhash_strings.bin"));
}

}  // namespace

int main() {
  test_insert_find_and_update();
  test_iterators_visit_entries();
  test_heterogeneous_string_lookup();
  test_custom_allocator_instantiation();
  test_operator_brackets_and_clear();
  test_try_emplace_supports_non_default_constructible_values();
  test_tail_wraparound_collision_survives_rehash();
  test_erase_and_reuse_deleted_slots();
  test_growth_preserves_values();
  test_collision_heavy_workload();
  test_move_insert_support();
  test_copy_support();
  test_randomized_against_unordered_map();
  test_concurrent_wrapper();
  test_binary_roundtrip_for_trivial_types();
  test_binary_load_rejects_corrupt_or_trailing_data();
  test_binary_serialization_rejects_non_trivial_types();
  return 0;
}
