# Changelog

Notable changes to htm, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); entries are dated because the
library has no releases.

## 2026-11-01

### Changed

- The README follows the family README standard: the description as its first sentence,
  Quick start before Installation, a pinned FetchContent tag, "Not for" limits, and the
  current `htm_bench` numbers. The diagram draws nested trixels in place of a 2186 x 450
  chain of names; it renders at 863 x 500.

## 2026-10-31

### Fixed

- `getId(Cartesian)` returns the same id on every build. The descent's products were
  left to the compiler, and a compiler that fuses a multiply into the add or subtract
  that uses it rounds once instead of twice: clang within one expression on any CPU
  with FMA (Apple Silicon, x86 with `-mfma` or `-march=haswell`), GCC by default on
  arm64 and with `-mfma`, and any compiler under `-ffp-contract=fast`. A point within
  rounding of a trixel edge then lands in the neighbouring trixel. Against the ids of
  plain IEEE-754 arithmetic (the code before the 2026-10-30 rewrite, built with
  `-ffp-contract=off`), the previous descent differed on 0.83% of random points, 0.65%
  of a one-degree latitude and longitude grid, 4.0% of special values and 57% of
  points on trixel edges (2.79 million points, Apple clang 17 on arm64; it also
  differs under Apple clang and GCC 16 with `-march=native` on x86-64). The
  differential test could not see it because its reference was built with the same
  flags in the same run. The descent now tells the compiler not to contract (clang's
  `fp contract(off)` pragma, GCC's `optimize` attribute, MSVC's `fp_contract` pragma);
  an x86 target without FMA has no instruction to fuse into and keeps the plain
  expressions. Clang ignores its pragma under an explicit `-ffp-contract=fast`, so the
  first call to `getId` runs the plain form on 48 points whose ids a fusing build gets
  wrong and, if any differs, uses a second form that rounds each product behind an
  empty `asm`. The ids are now those of a build without contraction: 0 of the 2.79
  million points differ under Apple clang 17 (default, `-ffp-contract=on`,
  `-ffp-contract=fast`), Homebrew clang 23 (`fast`) and GCC 16 (default,
  `-ffp-contract=fast`, `-ffp-contract=off`) on arm64, and 0 of 1.9 million under
  Apple clang 17 (default, `-mfma`, `-march=native`, `-march=native` with the default
  contraction) and GCC 16 (default, `-march=native`, with `-ffp-contract=fast` and with
  `-ffp-contract=off`) on x86-64. Audit H3. (`cf5177b`, `9006a84`)
- A build that fused (Apple clang and GCC on arm64, or x86 with FMA enabled) returned
  other ids for the points above. An index written by such a build files a point
  within rounding of a cell edge in the neighbouring cell; the points to reindex are
  those whose id differs between the old and the new build. Builds without
  contraction (x86-64 without FMA, `-ffp-contract=off`) return the same ids as before.

### Performance

`htm_bench`, ns per call, five interleaved runs, Apple clang 17, `-O3`. The fused
descent was faster because it did less work; the new one does the work the ids
require, at the speed of the same code built with `-ffp-contract=off` (592 ns on
arm64, 879 on x86 with FMA).

| row | build | before | after |
|---|---|---|---|
| `getId(Cartesian)` | arm64 (M4 Pro) | 559 | 593 |
| `getTrixelName(Cartesian)` | arm64 (M4 Pro) | 630 | 667 |
| `getId(Cartesian)` | x86-64 default (i9-9980HK) | 807 | 828 |
| `getTrixelName(Cartesian)` | x86-64 default | 969 | 982 |
| `getId(Cartesian)` | x86-64, `-march=haswell` | 798 | 873 |
| `getTrixelName(Cartesian)` | x86-64, `-march=haswell` | 945 | 1037 |
| `getTrixelName(id)` | arm64, x86-64 | unchanged | unchanged |

The reference walk of the start (`frozen` in the bench) takes 914 ns on arm64, so
`getId` is 1.54x faster than the start there, down from 1.63x. The first version of
the fix rounded each product behind an `asm` on every build and took 734 ns on arm64
and 901 ns on x86 with FMA; that form is now the fallback. A `*one` multiply with an
opaque 1.0 instead of the `asm` measured 695 ns on arm64 and 1012 ns on x86 with FMA,
and one `asm` per group of products 739 ns and 909 ns.

### Added

- `test/ieee.h`, an IEEE-754 reference for `getId` that rounds every operation through
  a `volatile`, and `test/golden.h`, 396 points with their ids (random points, poles,
  axes, trixel vertices and midpoints with one-ulp neighbours, and 96 points a fused
  build gets wrong). `test/gen_golden.cc` regenerates the table. `htm_diff` checks
  `getId` against both and against the `Cartesian`-object walk, and counts the points
  where the walk differs because the build's own `Cartesian` operators fuse.
- CTest `htm_fma` runs `htm_diff` against a second build of `htm.cc` with
  `-ffp-contract=fast` (and `-mfma` on x86 when the build machine runs it), and
  `htm_rounded` runs it with the rounded form of `getId` forced (`HTM_FORCE_ROUNDED`).

### Changed

- The CMake build pins cartesian at `49f4075`, the commit with the `std::hash<Cartesian>`
  equality fix. htm does not hash `Cartesian` values. (`cf99efe`)
- `test/diff_test.cc` runs 200,000 random points by default, down from 2,000,000, because
  each point now also runs the volatile reference.

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
