# Benchmarks

Build benchmarks when Google Benchmark is installed:

```bash
cmake -S . -B build -DPXHASH_BUILD_BENCHMARKS=ON
cmake --build build --target pxhash_bench
./build/pxhash_bench
```

For a quick smoke run:

```bash
./build/pxhash_bench --benchmark_min_time=0.01s
```

The benchmark currently includes:

- insert throughput for `PXHash`
- successful lookup throughput for `PXHash`
- missing lookup throughput for `PXHash`
- erase-heavy throughput for `PXHash`
- string-key insert and lookup throughput for `PXHash`
- comparable `std::unordered_map` workloads
- optional `absl::flat_hash_map` workloads when Abseil is found
- basic counters such as capacity, load factor, found count, and remaining count where applicable

Use Release builds for meaningful numbers. Avoid publishing benchmark claims from loaded developer machines; pin CPU, compiler, flags, data distribution, and load factor.
