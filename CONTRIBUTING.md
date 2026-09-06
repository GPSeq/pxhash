# Contributing

PXHash is a small C++20 header-only library. Keep changes focused and backed by tests.

Before opening a pull request:

```bash
cmake -S . -B build -DPXHASH_BUILD_BENCHMARKS=OFF
cmake --build build
ctest --test-dir build --output-on-failure
```

For API or probing changes, add randomized differential tests against `std::unordered_map`.

For performance claims, include compiler, CPU, build type, flags, benchmark command, and raw output.
