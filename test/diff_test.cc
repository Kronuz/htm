// Differential test: the optimized HTM::getId / getTrixelName must return
// exactly what the original Cartesian-object implementation returned.
//
// The "frozen" functions (frozen.h) are verbatim copies of the pre-optimization
// code (they build a Cartesian per midpoint). Inputs are random points on the
// sphere plus the cases that decide ties: poles, axis planes, signed zeros,
// start-trixel edges, and exact trixel vertices/midpoints at many levels.
//
// Usage: htm_diff_test [random_points]   (default 2,000,000)
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "frozen.h"
#include "htm.h"

static size_t checked = 0, failures = 0;

static void check(double x, double y, double z, bool names) {
	Cartesian c(x, y, z);
	++checked;
	uint64_t a = frozen_getId(c), b = HTM::getId(c);
	bool bad = a != b;
	if (names) {
		bad = bad || frozen_getTrixelName(c) != HTM::getTrixelName(c);
	}
	if (bad && ++failures <= 10) {
		std::printf("MISMATCH (%.17g %.17g %.17g): %llu vs %llu\n", x, y, z,
			(unsigned long long)a, (unsigned long long)b);
	}
}

static void check_normalized(double x, double y, double z, bool names) {
	double n = std::sqrt(x * x + y * y + z * z);
	if (n > 0) check(x / n, y / n, z / n, names);
}

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 2000000;
	std::mt19937_64 rng(12345);
	std::normal_distribution<double> g(0, 1);
	std::uniform_real_distribution<double> u(-1, 1);

	// Signed zeros, poles, axes, diagonals, and tiny components.
	const double vals[] = {0.0, -0.0, 1.0, -1.0, 0.5, -0.5, 1e-300, -1e-300, 1e-16, -1e-16, 0.7071067811865476, -0.7071067811865476};
	for (double x : vals) for (double y : vals) for (double z : vals) {
		check(x, y, z, true);
		check_normalized(x, y, z, true);
	}

	// Gaussian normalized (uniform on the sphere), and unnormalized, and a
	// cube-uniform (non-normalized) sample.
	for (size_t i = 0; i < n; ++i) {
		double x = g(rng), y = g(rng), z = g(rng);
		bool names = (i % 8) == 0;
		check_normalized(x, y, z, names);
		if ((i & 7) == 1) check(u(rng), u(rng), u(rng), names);
	}

	// Points on the start-trixel great-circle boundaries (an axis plane) and
	// on the edges of deeper trixels: a vertex or midpoint at each level, plus
	// slight perturbations of each.
	for (size_t i = 0; i < n / 8; ++i) {
		double a = g(rng), b = g(rng);
		int plane = i % 3;
		double p[3] = {a, b, 0};
		if (plane == 1) { p[2] = p[1]; p[1] = 0; }
		if (plane == 2) { p[2] = p[0]; p[0] = 0; }
		check_normalized(p[0], p[1], p[2], (i & 15) == 0);
	}
	std::uniform_int_distribution<int> pick(0, 7);
	for (size_t i = 0; i < n / 8; ++i) {
		const auto& t = start_trixels[pick(rng)];
		Cartesian v0 = start_vertices[t.v0], v1 = start_vertices[t.v1], v2 = start_vertices[t.v2];
		int lvl = std::uniform_int_distribution<int>(0, 24)(rng);
		for (int d = 0; d < lvl; ++d) {
			auto w2 = frozen_mid(v0, v1), w0 = frozen_mid(v1, v2), w1 = frozen_mid(v2, v0);
			switch (std::uniform_int_distribution<int>(0, 3)(rng)) {
				case 0: v1 = w2; v2 = w1; break;
				case 1: v0 = v1; v1 = w0; v2 = w2; break;
				case 2: v0 = v2; v1 = w1; v2 = w0; break;
				default: v0 = w0; v1 = w1; v2 = w2; break;
			}
		}
		auto m01 = frozen_mid(v0, v1), m12 = frozen_mid(v1, v2);
		const Cartesian pts[] = {v0, v1, v2, m01, m12};
		for (const auto& q : pts) {
			check(q.x, q.y, q.z, true);
			check_normalized(q.x + 1e-12 * g(rng), q.y + 1e-12 * g(rng), q.z + 1e-12 * g(rng), false);
		}
		// A point on the edge v0-v1 (a great-circle arc).
		double t01 = std::uniform_real_distribution<double>(0, 1)(rng);
		check_normalized(v0.x * (1 - t01) + v1.x * t01, v0.y * (1 - t01) + v1.y * t01, v0.z * (1 - t01) + v1.z * t01, false);
	}

	// getTrixelName(id): every start id, ids from the sampled points, and the
	// extremes of the valid range.
	for (uint64_t id = 8; id < 16; ++id) {
		if (frozen_name_from_id(id) != HTM::getTrixelName(id)) ++failures;
	}
	for (size_t i = 0; i < n; ++i) {
		int lvl = std::uniform_int_distribution<int>(0, 25)(rng);
		uint64_t id = (8 + (rng() & 7)) << (2 * lvl);
		id |= rng() & ((uint64_t(1) << (2 * lvl)) - 1);
		++checked;
		if (frozen_name_from_id(id) != HTM::getTrixelName(id) && ++failures <= 10) {
			std::printf("NAME MISMATCH %llu\n", (unsigned long long)id);
		}
	}

	std::printf("checked %zu, failures %zu\n", checked, failures);
	return failures ? 1 : 0;
}
