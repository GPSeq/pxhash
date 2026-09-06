#include "pxhash/pxhash.hpp"

#include <cstdint>
#include <iostream>

int main() {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map;
  map.insert(42, 100);

  if (const auto* value = map.find(42)) {
    std::cout << *value << '\n';
  }
}
