# API

PXHash is a header-only C++20 associative container.

```cpp
#include "pxhash/pxhash.hpp"
```

Primary type:

```cpp
pxhash::PXHash<Key, Value, Hash, Eq>
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

`operator[]` is available when `Value` is default-constructible.
