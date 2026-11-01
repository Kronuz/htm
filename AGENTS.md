# AGENTS.md

Working notes for agents modifying this repository. For the design read
`ARCHITECTURE.md`; for usage read `README.md`. This file covers the repo layout,
how to build and test, the invariants you must not break, and the traps that are
easy to fall into.

## Repo map

```
htm.h            HTM namespace API + trixel_t / range_t / index_t types + start tables. Includes constraint.h. From Xapiand src/geospatial/htm.h.
htm.cc           Naming descent, id packing, range conversion, set algebra, Constraint geometry helpers. From Xapiand src/geospatial/htm.cc (visualization tail dropped).
constraint.h     The Constraint spherical-cap class + earth-radius constants. From Xapiand src/geospatial/geometry.h (Constraint only).
constraint.cc    Constraint::set_data + comparison operators. From Xapiand src/geospatial/geometry.cc (Constraint only).
test/test.cc     Runnable smoke test: naming/id, lat/lon points, round trip, getCorners, getRange, set ops, Constraint + ranging helpers.
test/diff_test.cc  getId/getTrixelName against test/ieee.h (IEEE-754 reference), test/golden.h (ids) and test/frozen.h (the Cartesian-object walk). CTest `htm_diff` and `htm_fma`.
test/gen_golden.cc  Regenerates test/golden.h (not built).
examples/demo.cc A runnable tour (not a test).
CMakeLists.txt   STATIC library `htm` (+ alias htm::htm); FetchContent cartesian; CTest test `htm`.
LICENSE          MIT, Copyright (c) 2015-2019 Dubalu LLC.
README.md                    What it is, install, quick start.
docs/API.md                  Usage and the full surface: naming, ranges, set algebra, constraints.
docs/htm.d2                  The README diagram's source (docs/htm.svg is rendered from it).
ARCHITECTURE.md  Internal design: the mesh, naming, ids, ranges, set ops, Constraint.
```

This is not header-only. `htm.cc` and `constraint.cc` must be compiled and
linked; the headers only declare the API. The CMake target is a `STATIC` library
that links `cartesian` so `"cartesian.h"` resolves.

## Build and run the test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first configure fetches `cartesian` over the network (FetchContent, at the
commit `CMakeLists.txt` pins). Expected output ends with `all htm tests passed`, exit 0. The
test target is `htm_test`; the registered CTest name is `htm`. The test and demo
are only added when this repo is the top-level project (CMakeLists.txt), so
consumers vendoring it via `FetchContent` won't build them.

## Dependency

`htm.h` and `constraint.h` include `"cartesian.h"` for the `Cartesian` type and
the `PI_HALF` / `DBL_TOLERANCE` constants. That header lives in the sibling
[cartesian](https://github.com/Kronuz/cartesian) library, pulled in by CMake and
linked `PUBLIC` (Cartesian appears in the public interface, so consumers need it
too). It is pinned to a commit in `CMakeLists.txt`; move the pin when cartesian
changes something htm relies on. This is
the library's only dependency.

## Conventions

- **C++20.** The target requests `cxx_std_20` `PUBLIC`. The code uses
  `std::format`; don't drop the target below 20.
- **Filenames are stable.** `htm.h` / `htm.cc` keep their original Xapiand names
  so a consumer that already `#include`s the header just needs this repo on the
  include path. `constraint.h` / `constraint.cc` exist only here and are named to match.
- Tabs for indentation, double quotes in code, no em dashes in prose.
- MIT-licensed; keep the copyright header on source files.

## Load-bearing invariants

- **The mesh tables and bit layout are the contract.** `start_vertices`,
  `start_trixels`, `HTM_MAX_LEVEL`, and the `N`/`S` root bits (`10` / `11`)
  define what every name and id mean. Changing any of them changes every name and
  id this library produces and breaks compatibility with stored HTM keys. Don't
  touch them.
- **Naming and id descent must stay in lockstep.** `getTrixelName(coord)` and
  `getId(coord)` walk the same `insideVertex_Trixel` decision at each level;
  `getCorners` replays it from digits. If you change the child-selection logic in
  one, change it in all three, or names, ids, and corners disagree.
- **Set ops assume sorted input.** `trixel_union` / `trixel_intersection` /
  `trixel_exclusive_disjunction` and the `range_*` ops require their input
  vectors sorted ascending. They take their arguments by `&&` and move out of
  them. Don't pass unsorted data.
- **`Constraint` depends only on `Cartesian`.** That is why it lives here. Keep it
  that way; don't reach back into any `Geometry`/`Collection` types.

## How to extend

- **Always extend the smoke test.** `test/test.cc` is the only executable check.
  Add a case for any new entry point and assert the exact value, deriving it by
  tracing the code (or from a build), not by guessing. The existing values were
  derived that way.

## Traps

- **`getCorners` only validates digits past the root.** The loop runs over
  `name[2..len)`, so a bad character at index >= 2 throws `std::invalid_argument`,
  but a two-character root like `"N9"` is never checked (the loop doesn't run).
  Upstream behaves the same way; don't "fix" it without a reason.
- **`getId` doesn't validate.** `getId(name)` trusts its input and bit-packs
  whatever digits it sees. Garbage in, garbage out.
- **Ranges join adjacent integers.** `insertGreaterRange` and `simplifyRanges`
  treat `[a, b]` and `[b+1, c]` as mergeable (the `start - 1` comparisons). That is
  deliberate for contiguous id coverage; preserve it.

## Standalone vs. Xapiand

This is a standalone extraction from
[Xapiand](https://github.com/Kronuz/Xapiand). It differs from upstream in four
ways; the trixel naming, id, range, and set-operation results match upstream:

- **`Constraint` lives here.** Upstream keeps the spherical-cap class in
  `geometry.{h,cc}`; it depends only on `Cartesian`, so here it is
  `constraint.{h,cc}`, and `htm.h` does `#include "constraint.h"` instead of
  forward-declaring `class Constraint;`. `PI_HALF` / `DBL_TOLERANCE` come from
  `cartesian.h`; the earth-radius constants (`M_PER_RADIUS_EARTH`,
  `MAX_RADIUS_HALFSPACE_EARTH`, `MIN_RADIUS_METERS`, `MIN_RADIUS_RADIANS`,
  `RADIUS_GREAT_CIRCLE`) are in `constraint.h`.
- **No visualization writers.** Upstream's `htm.cc` ends with `writeGoogleMap` /
  `writeGoogleMapPlotter` / `writePython3D` / `writeGrahamScanMap` /
  `writeGrahamScan3D` and their `static` helpers, which write Google-Maps and
  matplotlib debug files and are the only thing coupling HTM to the
  `Geometry`/`Collection` hierarchy and `collection.h`. This copy has none of them,
  no matching declarations, and no `class Geometry;` forward declaration. No core
  function calls them.
- **`std::format` instead of `strings::format`.** `range_t::to_string` (header) and
  the error message in `getCorners` use `std::format`, and `getCorners` throws
  `std::invalid_argument` instead of Xapiand's `THROW`/`HTMError`. There is no
  `"strings.hh"` include.
- **Vector descent.** `getId(Cartesian)` descends on a plain three-double vector,
  and `getTrixelName(Cartesian)` derives the name from the id (see
  `ARCHITECTURE.md`). `getId` is IEEE-754 double arithmetic with every product
  rounded on its own, whatever the compiler's contraction mode: `htm.cc` keeps the
  descent in a plain form (compiler told not to contract) and a rounded form (`asm`
  barrier), and a first-call probe against 48 ids picks the plain form only if it is
  exact. Keep both forms and the probe table (`probes` in `htm.cc`) together; do not
  add an arithmetic expression to the descent outside `HTM_MUL`. `test/ieee.h` is the
  reference, `test/golden.h` the table of ids, and CTest `htm_fma` and `htm_rounded`
  run the same test against builds with `-ffp-contract=fast` (and `-mfma` on x86),
  the second with the rounded form forced (`HTM_FORCE_ROUNDED`).

Keep these reconcilable with upstream as plain edits.
