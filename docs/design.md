# Design

PXHash uses open addressing with SwissTable-style control bytes.

Each control byte is either:

- `EMPTY`
- `DELETED`
- a 7-bit fingerprint derived from the mixed hash

Lookup scans a group of control bytes at a time. On x86 builds with SSE2 or AVX2 available at compile time, SIMD compares are used to find candidate slots quickly. Otherwise PXHash falls back to a scalar group scan.

The control-byte array stores an extra mirrored tail of `GROUP_SIZE` bytes. This allows group loads near the end of the table while still mapping every candidate position back to the real circular slot range.

PXHash mixes the user-provided hash before using it for the probe index and fingerprint. This helps with weak integer hashes while still allowing custom hash functions.

The current table keeps slot storage separate from control bytes and constructs values only for occupied slots.
