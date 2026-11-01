# htm

Part of [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries), a collection of small, reusable C++ libraries.

[![CI](https://github.com/Kronuz/htm/actions/workflows/ci.yml/badge.svg)](https://github.com/Kronuz/htm/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)

Hierarchical Triangular Mesh for C++20: a 64-bit id for every point on the sphere, and an id range for every area.
The mesh tiles the sphere with nested triangles (trixels), and a point's trixel name grows
one digit per level, so a name prefix is an area and its id range holds everything inside
it. Reach for it to index points on the whole globe in a sorted key store, with no
projection seams.

![Nested trixels S, S1 and S13 around Sydney's trixel; getRange("S13") is the id range of every descendant](docs/htm.svg)

## ✨ Highlights

- **Names and ids.** `getTrixelName(point)` and `getId(point)` locate a point down to
  level 25, and `getTrixelName(id)` and `getId(name)` convert between the two forms.
- **Ranges for indexes.** `getRange(name)` is the id interval of a trixel's whole subtree,
  so an area query becomes integer range scans.
- **Set algebra.** Union, intersection and exclusive disjunction over sorted trixel names
  and over sorted id ranges.
- **Spherical caps.** `Constraint` is a circle on the sphere, with tests for a point
  inside it and for its overlap with a trixel.
- **Stable ids.** `getId` returns the same id whether or not the compiler fuses
  multiply-adds; CTest checks it against an IEEE-754 reference and a table of golden ids.
- **Whole-sphere coverage.** Poles and the date line need no special cases.

## 🚀 Quick start

```cpp
#include "htm.h"

#include <cstdio>

int main() {
	Cartesian sydney(-33.8688, 151.2093, 0.0, Cartesian::Units::DEGREES);
	std::string name = HTM::getTrixelName(sydney);   // "S13011231030232202122301131"
	uint64_t id = HTM::getId(sydney);                // 15506219993771101

	range_t area = HTM::getRange(name.substr(0, 3)); // everything inside "S13"
	std::printf("%d\n", area.start <= id && id <= area.end);   // 1
}
```

[`examples/demo.cc`](examples/demo.cc) zooms out by trimming a name and prints a trixel's
corners.

## 📦 Installation

Requires a C++20 compiler and CMake 3.14 or newer; it fetches
[cartesian](https://github.com/Kronuz/cartesian) at a pinned commit and compiles two source
files. CI builds it on Linux (x86_64, arm64), macOS (arm64, x86_64), and FreeBSD, plus
ASan, UBSan, and TSan builds.

```cmake
include(FetchContent)
FetchContent_Declare(htm
  GIT_REPOSITORY https://github.com/Kronuz/htm.git
  GIT_TAG        b4993b1fe793a3cb0fb525df23b9cc777ea76af6)  # or main, to track the tip
FetchContent_MakeAvailable(htm)

target_link_libraries(my_app PRIVATE htm::htm)
```

```cpp
#include "htm.h"
```

## 🎯 When to use it

Use it to index points on the globe in a sorted key store: one integer per point, and
range scans per area.

Not for:

- **Distances or projections.** HTM only partitions the sphere; use
  [cartesian](https://github.com/Kronuz/cartesian) for geometry.
- **Ready-made region covers.** The library tests a `Constraint` against a trixel; turning
  a circle into a list of trixels or ranges is the caller's walk.

## ⚡ Performance

ns per call on random unit vectors, five interleaved runs, Apple clang 17 with `-O3` and
default target flags:

| call | Apple M4 Pro | Intel Core i9-9980HK |
| --- | ---: | ---: |
| `getId(Cartesian)` | 593 | 828 |
| `getTrixelName(Cartesian)` | 667 | 982 |

The bench also times the reference `Cartesian`-object walk (`test/frozen.h`), which takes
914 ns per `getId` on the M4 Pro. Run it with:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target htm_bench
./build/htm_bench
```

## 📚 Documentation

- [docs/API.md](docs/API.md): naming, ranges, set algebra and constraints, with examples.
- [ARCHITECTURE.md](ARCHITECTURE.md): the subdivision, the id encoding and the range math.
- [CHANGELOG.md](CHANGELOG.md): dated changes, with the measurements behind each one.
- [AGENTS.md](AGENTS.md): repository map, invariants and traps.
- [Kronuz C++ Libraries](https://github.com/Kronuz/cpp-libraries): the catalog.

## 🛠️ Building and testing

```sh
cmake -B build
cmake --build build
ctest --test-dir build
```

CTest `htm_diff` checks `getId` and `getTrixelName` on random points, poles, signed zeros,
axis planes and trixel vertices and midpoints against an IEEE-754 reference
(`test/ieee.h`), golden ids (`test/golden.h`) and the `Cartesian`-object walk
(`test/frozen.h`). `htm_fma` and `htm_rounded` run the same test on builds of `htm.cc` with
`-ffp-contract=fast` (and `-mfma` on x86).

## 🤝 Contributing

Read [AGENTS.md](AGENTS.md) and the [catalog's rules](https://github.com/Kronuz/cpp-libraries/blob/main/AGENTS.md)
first; ids are stored in indexes, so the encoding does not change.

## 📄 License

MIT, Copyright (c) 2026 Germán Méndez Bravo and (c) 2015-2020 Dubalu LLC. See
[LICENSE](LICENSE). Extracted from [Xapiand](https://github.com/Kronuz/Xapiand), where it
backs geospatial search.
