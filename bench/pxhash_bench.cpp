#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "pxhash/pxhash.hpp"

#if HAVE_ABSL
  #include "absl/container/flat_hash_map.h"
#endif

namespace {

constexpr std::size_t kTotalItems = 1'000'000;

std::vector<std::uint64_t> generate_keys(std::size_t n) {
  std::vector<std::uint64_t> keys(n);
  std::mt19937_64 rng(12345);
  std::uniform_int_distribution<std::uint64_t> dist;
  for (std::size_t i = 0; i < n; ++i) keys[i] = dist(rng);
  return keys;
}

const std::vector<std::uint64_t> kTestKeys = generate_keys(kTotalItems);

std::vector<std::string> generate_string_keys(std::size_t n) {
  std::vector<std::string> keys;
  keys.reserve(n);
  for (std::size_t i = 0; i < n; ++i) {
    keys.push_back("key:" + std::to_string(kTestKeys[i]));
  }
  return keys;
}

const std::vector<std::string> kStringKeys = generate_string_keys(kTotalItems);

void BM_PXHash_Insert(benchmark::State& state) {
  for (auto _ : state) {
    pxhash::PXHash<std::uint64_t, std::uint64_t> map(static_cast<std::size_t>(state.range(0)));
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      map.insert(kTestKeys[i], kTestKeys[i]);
    }
    benchmark::DoNotOptimize(map);
    state.counters["capacity"] = static_cast<double>(map.capacity());
    state.counters["load_factor"] = map.load_factor();
  }
}
BENCHMARK(BM_PXHash_Insert)->Arg(kTotalItems);

void BM_PXHash_FindSuccessful(benchmark::State& state) {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map(static_cast<std::size_t>(state.range(0)));
  for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
    map.insert(kTestKeys[i], kTestKeys[i]);
  }

  for (auto _ : state) {
    std::uint64_t found = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      std::uint64_t value = 0;
      if (map.find(kTestKeys[i], value)) ++found;
    }
    benchmark::DoNotOptimize(found);
    state.counters["found"] = static_cast<double>(found);
  }
}
BENCHMARK(BM_PXHash_FindSuccessful)->Arg(kTotalItems);

void BM_PXHash_FindMissing(benchmark::State& state) {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map(static_cast<std::size_t>(state.range(0)));
  for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
    map.insert(kTestKeys[i], kTestKeys[i]);
  }

  for (auto _ : state) {
    std::uint64_t found = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      std::uint64_t value = 0;
      if (map.find(kTestKeys[i] ^ 0xffff'ffff'ffff'ffffULL, value)) ++found;
    }
    benchmark::DoNotOptimize(found);
    state.counters["false_hits"] = static_cast<double>(found);
  }
}
BENCHMARK(BM_PXHash_FindMissing)->Arg(kTotalItems);

void BM_PXHash_EraseHeavy(benchmark::State& state) {
  for (auto _ : state) {
    pxhash::PXHash<std::uint64_t, std::uint64_t> map(static_cast<std::size_t>(state.range(0)));
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      map.insert(kTestKeys[i], kTestKeys[i]);
    }
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); i += 2) {
      map.erase(kTestKeys[i]);
    }
    benchmark::DoNotOptimize(map);
    state.counters["remaining"] = static_cast<double>(map.size());
  }
}
BENCHMARK(BM_PXHash_EraseHeavy)->Arg(kTotalItems);

void BM_PXHash_StringInsert(benchmark::State& state) {
  for (auto _ : state) {
    pxhash::PXHash<std::string, std::uint64_t> map(static_cast<std::size_t>(state.range(0)));
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      map.insert(kStringKeys[i], i);
    }
    benchmark::DoNotOptimize(map);
  }
}
BENCHMARK(BM_PXHash_StringInsert)->Arg(kTotalItems / 10);

void BM_PXHash_StringFindSuccessful(benchmark::State& state) {
  pxhash::PXHash<std::string, std::uint64_t> map(static_cast<std::size_t>(state.range(0)));
  for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
    map.insert(kStringKeys[i], i);
  }

  for (auto _ : state) {
    std::uint64_t found = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      std::uint64_t value = 0;
      if (map.find(kStringKeys[i], value)) ++found;
    }
    benchmark::DoNotOptimize(found);
    state.counters["found"] = static_cast<double>(found);
  }
}
BENCHMARK(BM_PXHash_StringFindSuccessful)->Arg(kTotalItems / 10);

void BM_StdUnorderedMap_Insert(benchmark::State& state) {
  for (auto _ : state) {
    std::unordered_map<std::uint64_t, std::uint64_t> map;
    map.reserve(static_cast<std::size_t>(state.range(0)));
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      map.emplace(kTestKeys[i], kTestKeys[i]);
    }
    benchmark::DoNotOptimize(map);
    state.counters["bucket_count"] = static_cast<double>(map.bucket_count());
  }
}
BENCHMARK(BM_StdUnorderedMap_Insert)->Arg(kTotalItems);

void BM_StdUnorderedMap_FindSuccessful(benchmark::State& state) {
  std::unordered_map<std::uint64_t, std::uint64_t> map;
  map.reserve(static_cast<std::size_t>(state.range(0)));
  for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
    map.emplace(kTestKeys[i], kTestKeys[i]);
  }

  for (auto _ : state) {
    std::uint64_t found = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      if (map.find(kTestKeys[i]) != map.end()) ++found;
    }
    benchmark::DoNotOptimize(found);
  }
}
BENCHMARK(BM_StdUnorderedMap_FindSuccessful)->Arg(kTotalItems);

#if HAVE_ABSL
void BM_AbslFlatHashMap_Insert(benchmark::State& state) {
  for (auto _ : state) {
    absl::flat_hash_map<std::uint64_t, std::uint64_t> map;
    map.reserve(static_cast<std::size_t>(state.range(0)));
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      map.emplace(kTestKeys[i], kTestKeys[i]);
    }
    benchmark::DoNotOptimize(map);
  }
}
BENCHMARK(BM_AbslFlatHashMap_Insert)->Arg(kTotalItems);

void BM_AbslFlatHashMap_FindSuccessful(benchmark::State& state) {
  absl::flat_hash_map<std::uint64_t, std::uint64_t> map;
  map.reserve(static_cast<std::size_t>(state.range(0)));
  for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
    map.emplace(kTestKeys[i], kTestKeys[i]);
  }

  for (auto _ : state) {
    std::uint64_t found = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(state.range(0)); ++i) {
      if (map.find(kTestKeys[i]) != map.end()) ++found;
    }
    benchmark::DoNotOptimize(found);
  }
}
BENCHMARK(BM_AbslFlatHashMap_FindSuccessful)->Arg(kTotalItems);
#endif

}  // namespace

BENCHMARK_MAIN();
