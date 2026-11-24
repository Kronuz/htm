# Start tables

`start_vertices` and `start_trixels`, the eight starting trixels and their six vertices, are declared
`extern` in `htm.h` and defined once in `htm.cc`. A translation unit that includes `htm.h` runs no
static initializer for them; `htm.cc` holds the one initializer, because `Cartesian` is not a literal
type yet. Making `Cartesian` constexpr would remove that last one too.

Measured with `clang++ -std=c++20 -O2`, arm64 macOS, `size -m` and `nm`: `a.cc` only includes the
header, `b.cc` and `c.cc` each read the tables, and `b+c+htm` is `ld -r` of the three objects.

| Object | htm:c4577b7 (tables defined in the header) | htm:e51e63e (extern, defined in htm.cc) |
|---|---|---|
| a.o, b.o, c.o, each | 192 B __StaticInit, 1 initializer | none |
| htm.o | 192 B, 1 initializer | 192 B, 1 initializer |
| b+c+htm linked | 576 B, 3 initializers | 192 B, 1 initializer |

A global constructor in another translation unit that reads the tables would now depend on
initialization order; nothing does (Xapiand reads them only at query time).
