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

`getId(coord)` walks the descent. `startTrixel` picks the root octant from the
signs of `coord.x/y/z`. Then, level by level, the three edge midpoints are computed
(`midPoint` averages two vertices and re-normalizes), and `insideVertex_Trixel` (a
sign-of-cross-products test) decides which of the four children contains the point.
`getId` shifts left two bits and adds the child's digit. It runs a fixed
`HTM_MAX_LEVEL` iterations, so every point gets a full-depth id.
`getTrixelName(coord)` is `getTrixelName(getId(coord))`.

The descent runs on a plain three-double vector, not on `Cartesian` objects (which
carry a scale and an SRID). Each operation repeats the expression of `Cartesian`'s
`+`, `^`, `*`, and `normalize()` in the same order, in plain IEEE-754 double
arithmetic: every product is rounded before it is added or subtracted.

A compiler may contract `a * b + c` into one fused multiply-add, which rounds once.
It does so by default in GCC and in clang's `-ffp-contract=fast`, and clang does it
within one expression on any CPU with FMA. A point within rounding of a trixel edge
then lands in another trixel, and ids are stored in indexes, so an id must not
depend on the build.

Where the CPU has no fused multiply-add (x86-64 without FMA), nothing can fuse and
`htm.cc` uses the plain expressions. Elsewhere `htm.cc` holds the descent twice, as
`descend<false>` and `descend<true>`. The plain form tells the compiler not to
contract: clang's `fp contract(off)` pragma, GCC's `optimize("-ffp-contract=off")`
attribute, MSVC's `fp_contract` pragma. Clang ignores its pragma under an explicit
`-ffp-contract=fast`, and another compiler or version may not honor its control, so
the first call to `getId` runs the plain form on 48 points whose ids a fusing build
gets wrong, and uses it only if every id matches. Otherwise `getId` uses the form
whose products are rounded by `rounded_mul`, an empty `asm` the optimizer cannot see
through, which leaves nothing to fuse and costs about a quarter of the speed. Either
way every build returns the same id. Neither form holds under `-ffast-math`, which
may reassociate the sums: do not compile `htm.cc` with it.

The ids are the ones a build without contraction computes: x86-64 without FMA, or
any build with `-ffp-contract=off`. Only `getId(Cartesian)` is covered. The corner
and constraint geometry (`getCorners`, `Constraint`, the range search) runs on
`Cartesian` objects and follows the compiler's contraction mode; it sets which
ranges a query covers, never which id a point receives.

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
Xapiand's geometry layer) descends the mesh, keeping trixels fully
inside, recursing into partial ones, and pruning trixels whose bounding circle
misses the constraint.

`Constraint` depends only on `Cartesian`, which is why it lives here with HTM
rather than with the rest of Xapiand's geometry types.

## Why this shape

The naming and id encoding are the whole point: they make spatial locality a
string/integer prefix, so a spatial query becomes prefix matching or range
lookup in whatever store you already have. The library stops at that core plus
the cap primitive. It deliberately does not pull in the `Geometry`/`Collection`
class hierarchy that upstream uses only for debug visualization, so the
dependency is just `Cartesian`.
