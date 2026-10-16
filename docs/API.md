# htm API reference

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

## What the library provides

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
