# Thread Safety

PXHash is not a concurrent hash table.

Rules:

- Multiple threads may read the same table only when no thread mutates it.
- Any mutation must be externally synchronized.
- References and pointers returned by `find` can be invalidated by `insert`, `erase`, `clear`, `reserve`, and rehashing.

If concurrency becomes a project goal, the next design should be explicit: either a sharded locking wrapper, a read-mostly snapshot design, or a separate lock-free container. The current container should not be advertised as concurrent.
