# Architecture

The internals of `htm`: how the Hierarchical Triangular Mesh names a point, how
names and ids relate, how ranges and set operations work, and what role
`Constraint` plays. For usage see `README.md`; for the repo map and invariants
see `AGENTS.md`.

## Shape

Two headers and two compiled translation units:

```
  htm.h          The HTM namespace: trixel naming/id/range/set-op API, plus the
                 trixel_t / range_t / index_t types and the start-trixel tables.
  htm.cc         The implementation: naming descent, id bit-packing, range
                 conversion, set algebra, and the Constraint geometry helpers.
  constraint.h   The Constraint class (a spherical cap) and its earth-radius
                 constants. Included by htm.h.
  constraint.cc  Constraint's out-of-line methods (set_data, comparisons).
```

`htm.h` includes `constraint.h`, which includes `cartesian.h`. So a consumer
includes only `htm.h` and gets everything.

## The mesh

The sphere starts as eight spherical triangles: four around the north pole
(`N0..N3`) and four around the south (`S0..S3`), with vertices at the six axis
points `(±1, 0, 0)`, `(0, ±1, 0)`, `(0, 0, ±1)`. These are the `start_vertices`
and `start_trixels` tables in `htm.h`. Each trixel is then subdivided into four
children by joining the midpoints of its three edges (projected back onto the
sphere): the central child and the three corner children, numbered `0..3`.

A trixel **name** records that path: the `N`/`S` root, then one digit per level.
At the maximum level (`HTM_MAX_LEVEL == 25`, ~30 cm on Earth) a name is 27
characters. The **id** packs the same path into a 64-bit integer: two leading
bits for the root (`10` for `N`, `11` for `S`) and two bits per level for each
`0..3` digit.

## Naming a point

`getTrixelName(coord)` and `getId(coord)` walk the same descent. `startTrixel`
picks the root octant from the signs of `coord.x/y/z`. Then, level by level, the
three edge midpoints are computed (`midPoint` averages two vertices and
re-normalizes), and `insideVertex_Trixel` (a sign-of-cross-products test) decides
which of the four children contains the point. `getTrixelName` appends the digit;
`getId` shifts left two bits and adds it. Both run a fixed `HTM_MAX_LEVEL`
iterations, so every point gets a full-depth name.

`getCorners(name)` replays the descent from the digits instead of from a point,
returning the three vertices of the named trixel. A digit outside `0..3` past the
root is a malformed name and throws `std::invalid_argument`.

## Names, ids, and ranges

`getId(name)` and `getTrixelName(id)` convert between the two encodings directly
by walking the characters / bits. `getId(coord)` and `getId(name)` agree for the
name that `getTrixelName(coord)` produces.

A trixel at level `L` is the ancestor of every trixel beneath it, and because the
id packs the path in order, that whole subtree is a contiguous id interval.
`getRange(id, level)` computes it by shifting: the start is `id` shifted up to the
max level, the end is start plus the span of the subtree minus one. `getRange(name)`
derives the level from the name length. `getTrixels` / `getIdTrixels` go the other
way, decomposing a list of `range_t` intervals back into the minimal set of
trixels (or trixel ids) that exactly cover them.

## Set operations

Trixel-string set ops work on **sorted** vectors. `trixel_union` is a sorted
merge. `trixel_intersection` is prefix-aware: a trixel is in the intersection if
one side contains it or an ancestor of it, so `"N0"` on one side keeps `"N00"`
from the other. `trixel_exclusive_disjunction` keeps what is in exactly one side,
using a helper that subtracts a covered descendant from its ancestor by emitting
the sibling trixels. The `range_*` functions are the integer-interval analogues
over sorted `range_t` vectors, with `insertGreaterRange` joining adjacent or
overlapping intervals as they are appended. `simplifyTrixels` / `simplifyRanges`
collapse redundant entries (a descendant absorbed by its ancestor, four siblings
merged into their parent).

## Constraint

A `Constraint` is a spherical cap: a `center` direction, an `arcangle`
(half-aperture), the plane `distance` (`cos(arcangle)`), a `radius` in meters,
and a `Sign` recording whether the cap is the smaller half-space (`POS`), the
larger (`NEG`), or a great circle (`ZERO`). `set_data` derives the angle from a
metre radius using the earth radius, clamping to a minimum cap and to the whole
earth. The single-argument constructor builds a great circle through a center.

HTM uses constraints to range a circular region: `getBoundingCircle` returns the
cap that circumscribes a trixel, `intersectConstraints` tests two caps,
`insideVertex_Constraint` tests a point against a cap, and `intersection` tests a
cap against a trixel edge. A region-to-trixels search (built on top of these in
the original Xapiand geometry layer) descends the mesh, keeping trixels fully
inside, recursing into partial ones, and pruning trixels whose bounding circle
misses the constraint.

`Constraint` depends only on `Cartesian`, which is why it lives here with HTM
rather than with the rest of Xapiand's geometry types.

## Why this shape

The naming and id encoding are the whole point: they make spatial locality a
string/integer prefix, so a spatial query becomes prefix matching or range
lookup in whatever store you already have. The library stops at that core plus
the cap primitive. It deliberately does not pull in the `Geometry`/`Collection`
class hierarchy that the original file used only for debug visualization, so the
dependency is just `Cartesian`.
