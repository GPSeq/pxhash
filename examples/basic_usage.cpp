#include "pxhash/pxhash.hpp"

#include <cstdint>
#include <iostream>
#include <string>

int main() {
  pxhash::PXHash<std::string, std::uint64_t> counts;
  counts.insert("chr1:12345", 100);
  counts.insert_or_assign("chr1:12345", 101);
  counts.try_emplace("chr2:999", 200);

  if (const std::uint64_t* value = counts.find("chr1:12345")) {
    std::cout << "chr1:12345 => " << *value << '\n';
  }

  counts.erase("chr2:999");
  return counts.contains("chr2:999") ? 1 : 0;
}
