// IEEE-754 reference for HTM::getId: the same descent as frozen.h with every
// floating-point operation rounded on its own. Each intermediate result passes
// through a volatile, so no compiler setting (-ffp-contract=fast, FMA targets) can
// fuse a multiply into the add or subtract that uses it. HTM::getId must return
// this id on every build; the differential test and the golden table hold it to it.
#pragma once

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


inline bool inside(const V& v, const V& v0, const V& v1, const V& v2) {
	return triple(v0, v1, v) > 0 && triple(v1, v2, v) > 0 && triple(v2, v0, v) > 0;
}


inline uint64_t getId(const Cartesian& coord) {
	const auto& t = HTM::startTrixel(coord);
	V v0{start_vertices[t.v0].x, start_vertices[t.v0].y, start_vertices[t.v0].z};
	V v1{start_vertices[t.v1].x, start_vertices[t.v1].y, start_vertices[t.v1].z};
	V v2{start_vertices[t.v2].x, start_vertices[t.v2].y, start_vertices[t.v2].z};
	const V c{coord.x, coord.y, coord.z};
	uint64_t id = t.id;
	for (size_t depth = 0; depth < HTM_MAX_LEVEL; ++depth) {
		V w2 = mid(v0, v1), w0 = mid(v1, v2), w1 = mid(v2, v0);
		id <<= 2;
		if (inside(c, v0, w2, w1)) {
			v1 = w2; v2 = w1;
		} else if (inside(c, v1, w0, w2)) {
			id += 1; v0 = v1; v1 = w0; v2 = w2;
		} else if (inside(c, v2, w1, w0)) {
			id += 2; v0 = v2; v1 = w1; v2 = w0;
		} else {
			id += 3; v0 = w0; v1 = w1; v2 = w2;
		}
	}
	return id;
}

}  // namespace ieee
