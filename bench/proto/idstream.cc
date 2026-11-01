// Writes HTM::getId of a fixed stream of points to stdout (8 bytes each): random points
// uniform on the sphere, then points on and one ulp around the edges and inner edges of
// the trixels along random paths to level 25. Built once against the library at HEAD and
// once against the change, the two outputs must be identical.
//
// Usage: idstream [random_points [paths]] > ids.bin
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <random>
#include <vector>

#include "htm.h"

namespace {

struct V { double x, y, z; };

// The mesh's midpoint, every operation rounded on its own.
volatile double sink;
double r(double v) { sink = v; return sink; }
V mid(const V& a, const V& b) {
	double wx = r(a.x + b.x), wy = r(a.y + b.y), wz = r(a.z + b.z);
	double n = r(std::sqrt(r(r(r(wx * wx) + r(wy * wy)) + r(wz * wz))));
	return {r(wx / n), r(wy / n), r(wz / n)};
}

std::vector<uint64_t> out;
void emit(double x, double y, double z) {
	out.push_back(HTM::getId(Cartesian(x, y, z)));
	if (out.size() == 1 << 20) {
		std::fwrite(out.data(), 8, out.size(), stdout);
		out.clear();
	}
}

void ulps(const V& p) {
	const double inf = std::numeric_limits<double>::infinity();
	emit(p.x, p.y, p.z);
	emit(std::nextafter(p.x, inf), p.y, p.z);
	emit(std::nextafter(p.x, -inf), p.y, p.z);
	emit(p.x, std::nextafter(p.y, inf), p.z);
	emit(p.x, std::nextafter(p.y, -inf), p.z);
	emit(p.x, p.y, std::nextafter(p.z, inf));
	emit(p.x, p.y, std::nextafter(p.z, -inf));
}

V unit(double x, double y, double z) {
	double n = std::sqrt(x * x + y * y + z * z);
	return {x / n, y / n, z / n};
}

}  // namespace

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 10000000;
	size_t paths = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 3000;
	std::mt19937_64 rng(20261101);
	std::normal_distribution<double> g(0, 1);
	std::uniform_real_distribution<double> u(0, 1);
	for (size_t i = 0; i < n; ++i) {
		V p = unit(g(rng), g(rng), g(rng));
		emit(p.x, p.y, p.z);
	}
	const V sv[6] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
	const int st[8][3] = {{1, 0, 4}, {4, 0, 3}, {3, 0, 2}, {2, 0, 1}, {1, 5, 2}, {2, 5, 3}, {3, 5, 4}, {4, 5, 1}};
	for (size_t i = 0; i < paths; ++i) {
		const auto& t = st[rng() % 8];
		V v0 = sv[t[0]], v1 = sv[t[1]], v2 = sv[t[2]];
		for (int level = 0; level <= 25; ++level) {
			V w2 = mid(v0, v1), w0 = mid(v1, v2), w1 = mid(v2, v0);
			const V edges[6][2] = {{v0, v1}, {v1, v2}, {v2, v0}, {w0, w1}, {w1, w2}, {w2, w0}};
			for (const auto& e : edges) {
				ulps(e[0]);
				ulps(mid(e[0], e[1]));
				for (int k = 0; k < 2; ++k) {
					double s = u(rng);
					ulps(unit(e[0].x * (1 - s) + e[1].x * s, e[0].y * (1 - s) + e[1].y * s, e[0].z * (1 - s) + e[1].z * s));
				}
			}
			switch (rng() % 4) {
				case 0: v1 = w2; v2 = w1; break;
				case 1: v0 = v1; v1 = w0; v2 = w2; break;
				case 2: v0 = v2; v1 = w1; v2 = w0; break;
				default: v0 = w0; v1 = w1; v2 = w2; break;
			}
		}
	}
	std::fwrite(out.data(), 8, out.size(), stdout);
	return 0;
}
