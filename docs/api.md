# API

PXHash is a header-only C++20 associative container.

```cpp
#include "pxhash/pxhash.hpp"
```

Primary type:

```cpp
pxhash::PXHash<Key, Value, Hash, Eq, Allocator>
```

Core operations:

- `insert(key, value)` inserts a new key or assigns an existing key. It returns `true` when a new key was inserted and `false` when an existing value was updated.
- `insert_or_assign(key, value)` is the explicit form of the same behavior.
- `try_emplace(key, args...)` inserts only when the key is absent.
- `find(key)` returns a pointer to the value or `nullptr`.
- `find(key, out)` copies the value into `out` and returns `true` when found.
- `contains(key)` tests for presence.
- `erase(key)` removes a key and returns whether anything was erased.
- `reserve(n)` allocates for at least `n` elements.
- `clear()` removes all entries while keeping allocated capacity.
- `size()`, `capacity()`, `empty()`, `load_factor()`, and `max_load_factor()` expose table state.
- `begin()`, `end()`, `cbegin()`, and `cend()` iterate occupied `Slot<Key, Value>` entries.
- `get_allocator()` returns the allocator object configured for slot storage.

`operator[]` is available when `Value` is default-constructible.

Transparent lookup is supported when the hash and equality types can accept the lookup key:

```cpp
struct StringHash {
  using is_transparent = void;
  std::size_t operator()(std::string_view value) const noexcept;
};

struct StringEqual {
  using is_transparent = void;
  bool operator()(std::string_view lhs, std::string_view rhs) const noexcept;
};

pxhash::PXHash<std::string, int, StringHash, StringEqual> map;
map.contains(std::string_view("alpha"));
```

For synchronized multi-threaded access, use:

```cpp
#include "pxhash/concurrent_pxhash.hpp"

pxhash::ConcurrentPXHash<Key, Value> concurrent_map;
```
