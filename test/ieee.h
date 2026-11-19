// IEEE-754 reference for HTM::getId: the same descent as frozen.h with every
// floating-point operation rounded on its own. Each intermediate result passes
// through a volatile, so no compiler setting (-ffp-contract=fast, FMA targets) can
// fuse a multiply into the add or subtract that uses it. HTM::getId must return
// this id on every build; the differential test and the golden table hold it to it.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "htm.h"

namespace ieee {

inline double r(double v) {
	volatile double t = v;
	return t;
}


struct V {
	double x, y, z;
};


inline V mid(const V& a, const V& b) {
	double wx = r(a.x + b.x), wy = r(a.y + b.y), wz = r(a.z + b.z);
	double n = r(std::sqrt(r(r(r(wx * wx) + r(wy * wy)) + r(wz * wz))));
	return {r(wx / n), r(wy / n), r(wz / n)};
}


// (a ^ b) * c
inline double triple(const V& a, const V& b, const V& c) {
	double cx = r(r(a.y * b.z) - r(b.y * a.z));
	double cy = r(r(a.z * b.x) - r(b.z * a.x));
	double cz = r(r(a.x * b.y) - r(b.x * a.y));
	return r(r(r(cx * c.x) + r(cy * c.y)) + r(cz * c.z));
}


// The child of a trixel that holds c: the first of 0..3 whose closed triangle holds it,
// else the one it is least outside of (the first on a tie). With open triangles
// (Closed = false, the ids before 2026-11-18) it is the first of 0..2 that holds c
// strictly, else 3.
template <bool Closed>
inline int child(const V& c, const V (&t)[4][3], size_t* nearest) {
	double m[4];
	for (int k = 0; k < 4; ++k) {
		m[k] = std::min({triple(t[k][0], t[k][1], c), triple(t[k][1], t[k][2], c), triple(t[k][2], t[k][0], c)});
	}
	if constexpr (!Closed) {
		for (int k = 0; k < 3; ++k) {
			if (m[k] > 0) return k;
		}
		return 3;
	}
	for (int k = 0; k < 4; ++k) {
		if (m[k] >= 0) return k;
	}
	int best = 0;
	for (int k = 1; k < 4; ++k) {
		if (m[k] > m[best]) best = k;
	}
	if (nearest) ++*nearest;
	return best;
}


// nearest, when given, counts the levels where rounding left the point in no child.
template <bool Closed = true>
inline uint64_t getId(const Cartesian& coord, size_t* nearest = nullptr) {
	const auto& t = HTM::startTrixel(coord);
	V v0{start_vertices[t.v0].x, start_vertices[t.v0].y, start_vertices[t.v0].z};
	V v1{start_vertices[t.v1].x, start_vertices[t.v1].y, start_vertices[t.v1].z};
	V v2{start_vertices[t.v2].x, start_vertices[t.v2].y, start_vertices[t.v2].z};
	const V c{coord.x, coord.y, coord.z};
	uint64_t id = t.id;
	for (size_t depth = 0; depth < HTM_MAX_LEVEL; ++depth) {
		V w2 = mid(v0, v1), w0 = mid(v1, v2), w1 = mid(v2, v0);
		const V children[4][3] = {{v0, w2, w1}, {v1, w0, w2}, {v2, w1, w0}, {w0, w1, w2}};
		int k = child<Closed>(c, children, nearest);
		id = (id << 2) + k;
		v0 = children[k][0]; v1 = children[k][1]; v2 = children[k][2];
	}
	return id;
}

}  // namespace ieee
