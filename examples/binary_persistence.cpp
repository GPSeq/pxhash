#include "pxhash/pxhash.hpp"

#include <cstdint>

int main() {
  pxhash::PXHash<std::uint64_t, std::uint64_t> map;
  map.insert(10, 100);
  map.insert(20, 200);

  if (!map.save_binary("table.pxh")) return 1;

  pxhash::PXHash<std::uint64_t, std::uint64_t> restored;
  if (!restored.load_binary("table.pxh")) return 1;
  return restored.contains(20) ? 0 : 1;
}
