# htm

A C++20 implementation of the Hierarchical Triangular Mesh (HTM): a spatial
index that tiles the sphere with nested triangles ("trixels") and gives every
point a name and an integer id. Extracted from
[Xapiand](https://github.com/Kronuz/Xapiand).

## What it is

HTM recursively subdivides the sphere into spherical triangles. Each trixel has
a name (an `N`/`S` root plus one digit `0..3` per subdivision level) and a
matching 64-bit id. Nearby points share a name prefix, so the name doubles as a
spatial key: trim digits to zoom out, and a trixel's id range covers its whole
subtree. That is what makes HTM useful as an index. You map a query region to a
set of trixels (or id ranges) and look points up by prefix or by range.

This library gives you the core of that:

- **Naming and ids.** `getTrixelName(coord)` / `getId(coord)` for a `Cartesian`
  direction, `getTrixelName(id)` / `getId(name)` to convert between the two
  forms, and `getCorners(name)` to get a trixel's three spherical vertices.
- **Ranges.** `getRange(name)` / `getRange(id, level)` turn a trixel into the
  `[start, end]` id interval it covers, and `getTrixels` / `getIdTrixels` turn a
  list of ranges back into trixels.
- **Set algebra.** `trixel_union` / `trixel_intersection` /
  `trixel_exclusive_disjunction` over sorted trixel-string vectors, and the
  `range_*` equivalents over sorted `range_t` vectors, so you can combine query
  regions.
- **Constraints.** `Constraint` is a spherical cap (a circular area cut off the
  sphere by a plane), the primitive used to range a circular region into
  trixels. The geometry helpers (`getBoundingCircle`, `intersectConstraints`,
  `insideVertex_Constraint`, `intersection`, ...) are exposed so you can build
  ranging on top.

It works on the unit sphere: `Cartesian` directions are normalized, so the mesh
is over a unary sphere, not a spheroid.

## Install

This is not header-only. It ships two compiled translation units, `htm.cc` and
`constraint.cc`, plus the headers `htm.h` and `constraint.h`. You build and link
the `.cc` files; the headers alone only declare the API. It requires C++20 (it
uses `std::format`).

It has one dependency: [cartesian](https://github.com/Kronuz/cartesian), the
sibling library that provides the `Cartesian` type and the `PI_HALF` /
`DBL_TOLERANCE` constants. CMake pulls it in for you.

With CMake `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
  htm
  GIT_REPOSITORY https://github.com/Kronuz/htm.git
  GIT_TAG        main
)
FetchContent_MakeAvailable(htm)

target_link_libraries(your_target PRIVATE htm::htm)
```

`CMakeLists.txt` requests `cxx_std_20` `PUBLIC` and links `cartesian` `PUBLIC`,
so `"cartesian.h"` resolves on the include path with no extra wiring on your
side. Then:

```cpp
#include "htm.h"   // pulls in constraint.h and cartesian.h
```

The headers keep their original filenames, so a codebase that already
`#include "htm.h"` just needs this repo on its include path.

## Usage

```cpp
#include "htm.h"

// Name and id for a point given as a lat/lon (degrees).
Cartesian syd(-33.8688, 151.2093, 0.0, Cartesian::Units::DEGREES);
std::string name = HTM::getTrixelName(syd);   // "S13011231030232202122301131"
uint64_t    id   = HTM::getId(syd);           // 15506219993771101

// Convert between name and id.
uint64_t    same_id   = HTM::getId(name);     // == id
std::string same_name = HTM::getTrixelName(id);  // == name

// A trixel's three spherical-triangle vertices.
auto [v0, v1, v2] = HTM::getCorners("N3");    // (0,1,0) (0,0,1) (1,0,0)

// The id range a trixel covers (its whole subtree at the max level).
range_t r = HTM::getRange("N3");              // 12384898975268864 .. 13510798882111487

// Combine two sorted trixel sets.
std::vector<std::string> a{"N00", "N01", "N3"};
std::vector<std::string> b{"N01", "N02", "S0"};
auto both = HTM::trixel_intersection(std::move(a), std::move(b));  // {"N01"}

// A 1 km spherical cap around a point, the primitive for circular ranging.
Cartesian p(1.0, 0.0, 0.0);
Constraint cap(p, 1000.0);                    // radius in meters
```

## Build & test

```sh
cmake -B build && cmake --build build && ctest --test-dir build
```

The first configure fetches `cartesian` over the network. The test exercises
trixel naming and ids for known directions and lat/lon points, the name <-> id
round trip, `getCorners`, `getRange`, the trixel-set boolean operations, and the
`Constraint` cap plus the bounding-circle ranging helpers. It prints
`all htm tests passed` and exits 0.

## Examples

[`examples/demo.cc`](examples/demo.cc) is a runnable tour. A top-level CMake
build produces it next to the test:

```sh
cmake -B build && cmake --build build && ./build/htm_demo
```

It names a couple of real places, shows how trimming a name's digits zooms out
to the enclosing trixel, shows that the ancestor's id range brackets the leaf
id, and prints a trixel's three corners.

## Provenance

Extracted from [Xapiand](https://github.com/Kronuz/Xapiand), where this code
lived in `src/geospatial/htm.{h,cc}`. The extraction absorbed the `Constraint`
spherical-cap primitive (which had been sitting in Xapiand's `geometry.{h,cc}`
but depends only on `Cartesian`) into `constraint.{h,cc}`, dropped the
debug-only file-writing visualization methods that coupled HTM to Xapiand's
`Geometry`/`Collection` class hierarchy, and swapped Xapiand's `strings::format`
for `std::format`. The trixel naming, id, range, and set-operation core is
unchanged. See [ARCHITECTURE.md](ARCHITECTURE.md) for the design and
[AGENTS.md](AGENTS.md) for the repo map and invariants.

## License

MIT, Copyright (c) 2015-2019 Dubalu LLC. See [LICENSE](LICENSE).
