# Thread Safety

The base `PXHash` container is not internally synchronized.

Rules:

- Multiple threads may read the same table only when no thread mutates it.
- Any mutation must be externally synchronized.
- References and pointers returned by `find` can be invalidated by `insert`, `erase`, `clear`, `reserve`, and rehashing.

PXHash also provides `pxhash::ConcurrentPXHash`, a sharded locking wrapper for straightforward multi-threaded use.

`ConcurrentPXHash` rules:

- `insert`, `insert_or_assign`, `try_emplace`, `erase`, `clear`, and `reserve` synchronize the affected shard.
- `find(key, out)` copies the value while holding a shared shard lock.
- `contains` holds a shared shard lock.
- Pointer-returning lookup is intentionally not exposed by the concurrent wrapper.
