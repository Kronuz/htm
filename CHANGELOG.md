# Changelog

Notable changes to htm, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

## 2026-10-30

### Changed

- `getId(Cartesian)` runs the 25-level descent on a plain three-double vector that
  repeats the exact arithmetic of `Cartesian`'s `+`, `^`, `*`, and `normalize()`, so
  ids are bit-identical. Before, each level built three `Cartesian` objects for the
  midpoints, each with its SRID, scale, and an out-of-line constructor, which cost as
  much as the math. (`46094e9`)
- `getTrixelName(Cartesian)` derives the name from the id:
  `getTrixelName(getId(coord))`. Before, it ran its own descent. (`46094e9`)

### Added

- `test/diff_test.cc` (CTest `htm_diff`) compares `getId` and `getTrixelName` with
  the reference `Cartesian`-object implementation in `test/frozen.h` on random
  points, poles, signed zeros, axis planes, and trixel vertices and midpoints.
- `bench/bench.cc` (`htm_bench`) times both against the reference.
