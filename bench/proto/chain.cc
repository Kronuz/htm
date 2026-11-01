// Calibration: the latency of the fp midpoint chain along known digits vs getId.
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

#include "htm.h"

struct V3 { double x, y, z; };

static inline V3 mid(const V3& a, const V3& b) {
	V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
	double n = std::sqrt(w.x * w.x + w.y * w.y + w.z * w.z);
	return {w.x / n, w.y / n, w.z / n};
}

__attribute__((noinline)) static double chain(const Cartesian& c, uint64_t id) {
	const auto& t = HTM::startTrixel(c);
	V3 v0{start_vertices[t.v0].x, start_vertices[t.v0].y, start_vertices[t.v0].z};
	V3 v1{start_vertices[t.v1].x, start_vertices[t.v1].y, start_vertices[t.v1].z};
	V3 v2{start_vertices[t.v2].x, start_vertices[t.v2].y, start_vertices[t.v2].z};
	for (int k = 24; k >= 0; --k) {
		int d = (id >> (2 * k)) & 3;
		V3 w2 = mid(v0, v1), w0 = mid(v1, v2), w1 = mid(v2, v0);
		// branch-free select
		V3 n0 = d == 0 ? v0 : d == 1 ? v1 : d == 2 ? v2 : w0;
		V3 n1 = d == 0 ? w2 : d == 1 ? w0 : d == 2 ? w1 : w1;
		V3 n2 = d == 0 ? w1 : d == 1 ? w2 : d == 2 ? w0 : w2;
		v0 = n0; v1 = n1; v2 = n2;
	}
	return v0.x + v1.y + v2.z;
}

int main() {
	std::mt19937_64 rng(42);
	std::normal_distribution<double> g;
	std::vector<Cartesian> pts;
	std::vector<uint64_t> ids;
	for (int i = 0; i < 20000; ++i) {
		double x = g(rng), y = g(rng), z = g(rng);
		pts.emplace_back(x, y, z);
		pts.back().normalize();
		ids.push_back(HTM::getId(pts.back()));
	}
	for (int r = 0; r < 5; ++r) {
		auto t0 = std::chrono::steady_clock::now();
		uint64_t s = 0;
		for (auto& p : pts) s += HTM::getId(p);
		auto t1 = std::chrono::steady_clock::now();
		double acc = 0;
		for (size_t i = 0; i < pts.size(); ++i) acc += chain(pts[i], ids[i]);
		auto t2 = std::chrono::steady_clock::now();
		std::printf("getId %.1f  chain %.1f  (%llu %g)\n",
			std::chrono::duration<double, std::nano>(t1 - t0).count() / pts.size(),
			std::chrono::duration<double, std::nano>(t2 - t1).count() / pts.size(), (unsigned long long)s, acc);
	}
}
