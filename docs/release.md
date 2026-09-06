# Release Process

PXHash uses semantic versioning.

Before tagging:

1. Run tests on Linux, macOS, and Windows through GitHub Actions.
2. Run sanitizer checks.
3. Run benchmark smoke checks.
4. Build Doxygen documentation.
5. Update `CHANGELOG.md`.
6. Confirm the CMake package installs and can be consumed with `find_package(pxhash CONFIG REQUIRED)`.

Tag format:

```bash
git tag -a v0.1.0 -m "PXHash 0.1.0"
git push origin v0.1.0
```

Release notes should include API changes, compatibility notes, benchmark environment, and migration notes.
