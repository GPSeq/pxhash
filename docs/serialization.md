# Serialization

PXHash supports binary save/load when both key and value are trivially copyable.

```cpp
pxhash::PXHash<std::uint64_t, std::uint64_t> map;
map.insert(10, 100);
map.save_binary("table.pxh");

pxhash::PXHash<std::uint64_t, std::uint64_t> restored;
restored.load_binary("table.pxh");
```

Compatibility limits:

- The format is PXHash-specific.
- It is intended for compatible builds and architectures.
- It is not a portable interchange format.
- Non-trivially-copyable types such as `std::string` are rejected.

The loader validates the magic value, version, reserved header field, full record reads, duplicate-entry count, and trailing bytes.
