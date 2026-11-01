# htm performance

`bench/bench.cc` (`htm_bench`) times each library on the same 20,000 random unit vectors
(a normal vector in 3-D, normalized; seed 42). Every row is one call per point, ns per call.
Five rounds run in turn, every row once per round, so drift reaches all rows alike; a run
prints the best, median and worst round of each row. The README quotes the best of the
two runs of the bench on each machine. `htm (A/A)` is the same `getId` loop as `htm`
timed as a second row: it measures the noise of the method, 0.2% and 0.7% from `htm` on the
M4 Pro and 0.3% and 1.9% on the i9 (best of five, in the two runs).

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release -DHTM_BENCH_COMPARE=ON -DHTM_BENCH_S2=ON
cmake --build build --target htm_bench
./build/htm_bench [points [rounds]]
```

Apple clang 17 builds every library with `-O3 -DNDEBUG` (CMake `Release`); Apple M4 Pro on
macOS 26 and Intel Core i9-9980HK on macOS 15 (x86-64).

## What each row times

| row | call | input type |
| --- | --- | --- |
| `htm` | `HTM::getId(const Cartesian&)`, `HTM::getTrixelName(const Cartesian&)` | `Cartesian` (unit vector) |
| `SDSS HTM` | `SpatialIndex::idByPoint`, `nameByPoint` into a caller's buffer, index of level 25 | `SpatialVector` (x, y, z) |
| `H3 res 15` | `latLngToCell(&ll, 15, &cell)` | `LatLng` in radians (`asin z`, `atan2 y x`) |
| `S2 level 25` | `S2CellId(point).parent(25).id()` | `S2Point` (x, y, z) |

The conversions run once before timing and are not in any row. `getTrixelName` in htm
returns a `std::string` (one allocation); SDSS `nameByPoint` fills a buffer.

The comparators are built by the bench target only, with the same compiler and flags:

| library | version | notes |
| --- | --- | --- |
| SDSS HTM | [gkhtm e319c99](https://github.com/genghisken/gkhtm/tree/e319c997a98019302428ca5667b288ee9c869deb) | `SpatialIndex(25)`, which builds the first 5 levels as a table and descends the rest by name. The repository is MIT; the vendored Johns Hopkins University sources carry a copyright header and no license text, so the bench fetches them at configure time and does not redistribute them |
| H3 | [v4.5.0](https://github.com/uber/h3/tree/v4.5.0), Apache-2.0 | static library, no filters or tests |
| Google S2 | [677c8ce](https://github.com/google/s2geometry/tree/677c8cebcff11ba11accc0b02005bc214e06e9fd), Apache-2.0 | the commit after v0.14.0 that builds against abseil 20260817.0; abseil from Homebrew, built as C++20 |

## Cell sizes

The bench prints each library's own average cell size:

| library | level | area | edge |
| --- | --- | ---: | ---: |
| htm | 25 | 0.0566 m² | 0.298 m |
| S2 | 25 | 0.0755 m² | 0.277 m |
| H3 | 15 | 0.8953 m² | 0.584 m |

The htm figures are the sphere (mean radius 6371.0088 km) over 8 x 4^25 trixels and the
root edge (a quarter of a great circle) over 2^25. H3 resolution 15 is the finest H3 has, so
its cells stay 16 times larger in area than htm's; S2 level 25 is the closest S2 level.

## SDSS HTM names the same mesh

SDSS and htm share the eight root trixels (`S0` to `S3` and `N0` to `N3` on the same vertices)
and the child numbering (corner at `v0`, `v1`, `v2`, then the middle). The ids differ in one
bit: SDSS numbers `S0` to `S3` as 8 to 11 and `N0` to `N3` as 12 to 15, htm the reverse
(`N0` is 8). At level 25, htm's id equals SDSS's with bit 52 flipped; the bench checks it
on every input whose names are equal.

The bench aborts when the first 14 levels of a name differ. On the 20,000 inputs the names
agree on all 25 levels for 10525 (M4 Pro) and 10517 (i9) of them. The difference is at the
deepest levels, where a trixel is 3e-8 radians wide: SDSS tests each sign with a tolerance
of 1e-15 (`gEpsilon`), a triple product there is about the edge length times the distance, and
the first child that accepts wins. A probe over 1,000,000 points, not part of the bench,
counted the points whose names differ within the first L levels:

| L | 14 | 16 | 18 | 20 | 22 | 25 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| points | 0 | 1 | 44 | 652 | 11088 | 476524 |

With `gEpsilon` set to 0 in a copy of the sources, 10199 of the 1,000,000 points still differ
at level 25: two double-precision implementations of the same test disagree near an edge.

## Apple M4 Pro, macOS 26

Best of the ten rounds, mean of the two runs' medians, worst of the ten rounds.

| call | library | best | median | worst |
| --- | --- | ---: | ---: | ---: |
| `getId` | htm | 591 | 600 | 610 |
| `getId` | htm (A/A) | 592 | 602 | 616 |
| `getTrixelName` | htm | 662 | 668 | 683 |
| `getId` | SDSS HTM | 962 | 990 | 1045 |
| `getTrixelName` | SDSS HTM | 962 | 995 | 1021 |
| `getId` | H3 res 15 | 497 | 507 | 517 |
| `getId` | S2 level 25 | 19.8 | 20.2 | 20.6 |

## Intel Core i9-9980HK, macOS 15

| call | library | best | median | worst |
| --- | --- | ---: | ---: | ---: |
| `getId` | htm | 795 | 815 | 883 |
| `getId` | htm (A/A) | 793 | 819 | 853 |
| `getTrixelName` | htm | 962 | 989 | 1082 |
| `getId` | SDSS HTM | 1517 | 1600 | 1820 |
| `getTrixelName` | SDSS HTM | 1542 | 1571 | 1611 |
| `getId` | H3 res 15 | 755 | 769 | 801 |
| `getId` | S2 level 25 | 36.2 | 37.1 | 46.3 |
