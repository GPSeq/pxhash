# PXHash

[![Tests](https://github.com/lutfia95/pxhash/actions/workflows/tests.yml/badge.svg?branch=main)](https://github.com/lutfia95/pxhash/actions/workflows/tests.yml)
[![Docs](https://github.com/lutfia95/pxhash/actions/workflows/docs.yml/badge.svg?branch=main)](https://github.com/lutfia95/pxhash/actions/workflows/docs.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![Doxygen](https://img.shields.io/badge/docs-Doxygen-2C4AA8)](https://www.doxygen.nl/)

High-performance C++20 open-addressing hash table.

PXHash is a compact header-only hash table inspired by SwissTable control-byte probing. It is designed for fast insert, lookup, erase, and binary persistence for trivially-copyable key/value pairs.

# Requirements

## Minimum

- C++20
- GCC >= 10, Clang >= 12, or MSVC with C++20 support
- CMake >= 3.16

## For benchmarking (optional but recommended)

- Google Benchmark
- Abseil

# Installation

## Ubuntu / Debian
### Core toolchain

```bash
sudo apt update
sudo apt install -y build-essential cmake pkg-config
sudo apt install -y libbenchmark-dev
sudo apt install -y libabsl-dev
```

## macOS / Homebrew

```bash
brew install cmake google-benchmark abseil
```

# Build

## Configure and build everything available

```bash
git clone https://github.com/lutfia95/pxhash.git
cd pxhash
cmake -S . -B build
cmake --build build -j
```

By default, CMake tries to generate:

- `pxhash_tests`
- `pxhash_bench`
- `pxhash_example_basic`
- `pxhash_example_custom_key`
- `pxhash_example_binary_persistence`

The benchmark target is only generated if Google Benchmark is installed and discoverable by CMake. If it is missing, configure will print a warning and only the test target will be created.

## Build options

```bash
cmake -S . -B build -DPXHASH_BUILD_TESTS=ON -DPXHASH_BUILD_BENCHMARKS=ON
```

Available options:

- `PXHASH_BUILD_TESTS=ON|OFF` controls `pxhash_tests`
- `PXHASH_BUILD_BENCHMARKS=ON|OFF` controls `pxhash_bench`
- `PXHASH_BUILD_EXAMPLES=ON|OFF` controls examples
- `PXHASH_ENABLE_AVX2=ON|OFF` enables AVX2 compile flags
- `PXHASH_NATIVE_ARCH=ON|OFF` enables native CPU compile flags on GCC/Clang

Examples:

Build tests only:

```bash
cmake -S . -B build -DPXHASH_BUILD_BENCHMARKS=OFF
cmake --build build -j
```

Build benchmarks only:

```bash
cmake -S . -B build -DPXHASH_BUILD_TESTS=OFF
cmake --build build -j
```

If CMake prints that `pxhash_bench` was skipped, install Google Benchmark first or configure with `-DPXHASH_BUILD_BENCHMARKS=OFF`.

## Run tests

```bash
ctest --test-dir build --output-on-failure
```

## Run benchmark

```bash
./build/pxhash_bench
```

# Container / Docker
```bash
docker build -t pxhash .
docker run --rm pxhash ctest --test-dir build --output-on-failure
```

Run the benchmark binary inside the container:

```bash
docker run --rm pxhash ./build/pxhash_bench
```

## Usage Example
```cpp
#include <iostream>
#include <string>
#include "pxhash/pxhash.hpp"

int main() {
    pxhash::PXHash<std::string, uint64_t> map;

    map.insert("chr1:12345", 100);
    map.insert_or_assign("chr2:999", 200);

    if (const uint64_t* value = map.find("chr1:12345")) {
        std::cout << "found chr1:12345 => " << *value << "\n";
    }

    map.erase("chr1:12345");
    return map.contains("chr1:12345") ? 1 : 0;
}
```

## Binary Persistence

`PXHash` can save to and load from a compact binary file when both `KeyType` and `ValueType` are trivially copyable, for example `uint64_t`, POD structs, or fixed-size IDs.

```cpp
#include <cstdint>
#include "pxhash/pxhash.hpp"

int main() {
    pxhash::PXHash<std::uint64_t, std::uint64_t> map;
    map.insert(10, 100);
    map.insert(20, 200);

    map.save_binary("table.pxh");

    pxhash::PXHash<std::uint64_t, std::uint64_t> restored;
    restored.load_binary("table.pxh");
}
```

Notes:

- The binary format is a small PXHash-specific header plus raw key/value records.
- This path intentionally rejects non-trivially-copyable types such as `std::string`.
- The file is intended for use on compatible builds and architectures; it is not a cross-platform interchange format.

## Tuning

Enable AVX2 explicitly:

```bash
cmake -S . -B build -DPXHASH_ENABLE_AVX2=ON
```

Compile local targets for the current CPU on GCC/Clang:

```bash
cmake -S . -B build -DPXHASH_NATIVE_ARCH=ON
```

Use both only when the resulting binary does not need to run on older CPUs.

## CMake Package

PXHash exports an interface target:

```cmake
find_package(pxhash CONFIG REQUIRED)
target_link_libraries(my_target PRIVATE pxhash::pxhash)
```

## License

MIT

## Roadmap

See [TODO.md](TODO.md) for the 13-step project checklist.

## Documentation

- [API](docs/api.md)
- [Design](docs/design.md)
- [Benchmarks](docs/benchmarks.md)
- [Thread safety](docs/thread_safety.md)
- [Serialization](docs/serialization.md)
