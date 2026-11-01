// Differential test of HTM::getId's descent against the IEEE-754 reference (ieee.h,
// the ids of the descent as it was before the predicted descent), concentrated where a
// descent can go wrong: on and next to the edges of the mesh, at every level.
//
// Classes, each against ieee::getId:
//   - random points, uniform on the sphere;
//   - every edge of every trixel down to level 5, and the edges of the trixels along
//     random paths down to level 25: the edge's vertices, its midpoint and points along
//     it, each with its twelve neighbours one ulp away in one coordinate;
//   - points at a range of distances from the inner edges (the edges between a
//     trixel's midpoints), across the margin the descent certifies its tests with;
//   - the root vertices, the root edge midpoints and the face centres, one ulp around;
//   - vectors that are not unit: scaled by powers of two up to the limits of the
//     range, tiny and subnormal components, zero, infinities and NaN.
//
// Usage: htm_descent_test [random_points [paths]]   (default 1,000,000 and 300)
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <vector>

#include "htm.h"
#include "ieee.h"

namespace {

size_t checked = 0, failures = 0;

void check(double x, double y, double z, const char* what) {
	++checked;
	Cartesian c(x, y, z);
	uint64_t want = ieee::getId(c), got = HTM::getId(c);
	if (got != want && ++failures <= 20) {
		std::printf("MISMATCH %s (%a, %a, %a): want %llu got %llu\n", what, x, y, z,
			(unsigned long long)want, (unsigned long long)got);
	}
}

// The point and its neighbours one ulp away in each coordinate.
void check_ulps(const ieee::V& p, const char* what) {
	check(p.x, p.y, p.z, what);
	const double inf = std::numeric_limits<double>::infinity();
	check(std::nextafter(p.x, inf), p.y, p.z, what);
	check(std::nextafter(p.x, -inf), p.y, p.z, what);
	check(p.x, std::nextafter(p.y, inf), p.z, what);
	check(p.x, std::nextafter(p.y, -inf), p.z, what);
	check(p.x, p.y, std::nextafter(p.z, inf), what);
	check(p.x, p.y, std::nextafter(p.z, -inf), what);
}

ieee::V normalized(double x, double y, double z) {
	double n = std::sqrt(x * x + y * y + z * z);
	return {x / n, y / n, z / n};
}

// Points on the great-circle arc from a to b: its ends, its fp midpoint and points
// along it, each with its one-ulp neighbours.
template <class Rng>
void check_edge(const ieee::V& a, const ieee::V& b, Rng& rng, int along) {
	check_ulps(a, "vertex");
	check_ulps(ieee::mid(a, b), "midpoint");
	std::uniform_real_distribution<double> u(0, 1);
	for (int i = 0; i < along; ++i) {
		double t = u(rng);
		check_ulps(normalized(a.x * (1 - t) + b.x * t, a.y * (1 - t) + b.y * t, a.z * (1 - t) + b.z * t), "edge");
	}
}

// The three edges of a trixel and the three edges between its midpoints.
template <class Rng>
void check_trixel(const ieee::V& v0, const ieee::V& v1, const ieee::V& v2, Rng& rng, int along) {
	ieee::V w2 = ieee::mid(v0, v1), w0 = ieee::mid(v1, v2), w1 = ieee::mid(v2, v0);
	check_edge(v0, v1, rng, along);
	check_edge(v1, v2, rng, along);
	check_edge(v2, v0, rng, along);
	check_edge(w0, w1, rng, along);
	check_edge(w1, w2, rng, along);
	check_edge(w2, w0, rng, along);
}

ieee::V start_vertex(int i) {
	return {start_vertices[i].x, start_vertices[i].y, start_vertices[i].z};
}

void child(int d, ieee::V& v0, ieee::V& v1, ieee::V& v2) {
	ieee::V w2 = ieee::mid(v0, v1), w0 = ieee::mid(v1, v2), w1 = ieee::mid(v2, v0);
	switch (d) {
		case 0: v1 = w2; v2 = w1; break;
		case 1: v0 = v1; v1 = w0; v2 = w2; break;
		case 2: v0 = v2; v1 = w1; v2 = w0; break;
		default: v0 = w0; v1 = w1; v2 = w2; break;
	}
}

template <class Rng>
void every_trixel(ieee::V v0, ieee::V v1, ieee::V v2, int level, int max_level, Rng& rng) {
	check_trixel(v0, v1, v2, rng, 1);
	if (level == max_level) return;
	for (int d = 0; d < 4; ++d) {
		ieee::V a = v0, b = v1, c = v2;
		child(d, a, b, c);
		every_trixel(a, b, c, level + 1, max_level, rng);
	}
}

// Points off the inner edge a-b by s times the arc's length, for s across sixteen
// decades: the predicted descent certifies the tests of the inner edges from the
// parent's edge tests when they clear a margin, and computes them otherwise.
template <class Rng>
void check_offsets(const ieee::V& a, const ieee::V& b, Rng& rng) {
	double nx = a.y * b.z - a.z * b.y, ny = a.z * b.x - a.x * b.z, nz = a.x * b.y - a.y * b.x;
	double len = std::sqrt(nx * nx + ny * ny + nz * nz);
	if (!(len > 0)) return;
	nx /= len; ny /= len; nz /= len;
	std::uniform_real_distribution<double> u(0.05, 0.95);
	for (int e = -20; e <= -4; ++e) {
		for (double sign : {-1.0, 1.0}) {
			double t = u(rng), s = sign * std::ldexp(u(rng) + 0.5, e) * len;
			double px = a.x * (1 - t) + b.x * t, py = a.y * (1 - t) + b.y * t, pz = a.z * (1 - t) + b.z * t;
			ieee::V p = normalized(px + s * nx, py + s * ny, pz + s * nz);
			check(p.x, p.y, p.z, "offset");
		}
	}
}

}  // namespace

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1000000;
	size_t paths = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 300;
	std::mt19937_64 rng(20261101);
	std::normal_distribution<double> g(0, 1);

	// Random points, uniform on the sphere.
	size_t before = failures;
	for (size_t i = 0; i < n; ++i) {
		ieee::V p = normalized(g(rng), g(rng), g(rng));
		check(p.x, p.y, p.z, "random");
	}
	std::printf("random: %zu points, %zu mismatches\n", n, failures - before);

	// Every trixel down to level 5, its edges and inner edges.
	before = failures;
	size_t c0 = checked;
	for (const auto& t : start_trixels) {
		every_trixel(start_vertex(t.v0), start_vertex(t.v1), start_vertex(t.v2), 0, 5, rng);
	}
	std::printf("edges of every trixel to level 5: %zu points, %zu mismatches\n", checked - c0, failures - before);

	// Random paths to level 25: at every level the trixel's edges, inner edges and offsets.
	before = failures;
	c0 = checked;
	std::uniform_int_distribution<int> pick(0, 7), digit(0, 3);
	for (size_t i = 0; i < paths; ++i) {
		const auto& t = start_trixels[pick(rng)];
		ieee::V v0 = start_vertex(t.v0), v1 = start_vertex(t.v1), v2 = start_vertex(t.v2);
		for (int level = 0; level <= (int)HTM_MAX_LEVEL; ++level) {
			check_trixel(v0, v1, v2, rng, 2);
			ieee::V w2 = ieee::mid(v0, v1), w0 = ieee::mid(v1, v2), w1 = ieee::mid(v2, v0);
			check_offsets(w1, w2, rng);
			check_offsets(w2, w0, rng);
			check_offsets(w0, w1, rng);
			child(digit(rng), v0, v1, v2);
		}
	}
	std::printf("edges along %zu paths to level 25: %zu points, %zu mismatches\n", paths, checked - c0, failures - before);

	// Root vertices, root edge midpoints and face centres, one ulp around.
	before = failures;
	c0 = checked;
	for (int i = 0; i < 6; ++i) check_ulps(start_vertex(i), "root vertex");
	for (const auto& t : start_trixels) {
		ieee::V a = start_vertex(t.v0), b = start_vertex(t.v1), c = start_vertex(t.v2);
		check_ulps(ieee::mid(a, b), "root midpoint");
		check_ulps(ieee::mid(b, c), "root midpoint");
		check_ulps(ieee::mid(c, a), "root midpoint");
		check_ulps(normalized(a.x + b.x + c.x, a.y + b.y + c.y, a.z + b.z + c.z), "face centre");
	}
	std::printf("root vertices, midpoints, centres: %zu points, %zu mismatches\n", checked - c0, failures - before);

	// Vectors that are not unit, and the special values.
	before = failures;
	c0 = checked;
	const double inf = std::numeric_limits<double>::infinity(), nan = std::numeric_limits<double>::quiet_NaN();
	for (int i = 0; i < 2000; ++i) {
		ieee::V p = normalized(g(rng), g(rng), g(rng));
		for (int e : {-1074, -1060, -1022, -1000, -600, -501, -500, -499, -300, -10, 1, 10, 300, 499, 500, 501, 600, 1000, 1023}) {
			check(std::ldexp(p.x, e), std::ldexp(p.y, e), std::ldexp(p.z, e), "scaled");
		}
		check(p.x, p.y * 1e-300, p.z, "tiny component");
		check(p.x * 1e-320, p.y, p.z * 1e-310, "subnormal components");
	}
	const double vals[] = {0.0, -0.0, 1.0, -1.0, 0x1p-500, 0x1p500, 0x1p-1074, inf, -inf, nan, 1e308, -1e-308};
	for (double x : vals) for (double y : vals) for (double z : vals) check(x, y, z, "special");
	std::printf("scaled and special values: %zu points, %zu mismatches\n", checked - c0, failures - before);

	std::printf("checked %zu, failures %zu\n", checked, failures);
	return failures ? 1 : 0;
}
