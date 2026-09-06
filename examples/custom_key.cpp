#include "pxhash/pxhash.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>

struct Coordinate {
  std::uint32_t chromosome;
  std::uint64_t position;
};

struct CoordinateHash {
  std::size_t operator()(const Coordinate& c) const noexcept {
    return static_cast<std::size_t>((c.position << 8) ^ c.chromosome);
  }
};

struct CoordinateEqual {
  bool operator()(const Coordinate& a, const Coordinate& b) const noexcept {
    return a.chromosome == b.chromosome && a.position == b.position;
  }
};

int main() {
  pxhash::PXHash<Coordinate, std::uint64_t, CoordinateHash, CoordinateEqual> map;
  map.insert(Coordinate{1, 12345}, 7);

  if (const auto* value = map.find(Coordinate{1, 12345})) {
    std::cout << *value << '\n';
  }
}
