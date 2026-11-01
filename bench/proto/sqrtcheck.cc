// Where the exponent-halving square root is exact, and how far from 4 the descent's
// sums of squares get at each level of the mesh.
#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma clang fp contract(off)

static inline uint64_t bits(double d) { uint64_t b; std::memcpy(&b, &d, 8); return b; }
static inline double from(uint64_t b) { double d; std::memcpy(&d, &b, 8); return d; }
static inline double sqrt_near4(double q) { return from((bits(q) >> 1) + 0x1FF8000000000000ULL); }

struct V3 { double x, y, z; };
static double qmax_level[32];
static int64_t dmin_level[32], dmax_level[32];

static inline V3 mid(const V3& a, const V3& b, int level) {
	V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
	double q = w.x * w.x + w.y * w.y + w.z * w.z;
	int64_t d = (int64_t)bits(q) - (int64_t)bits(4.0);
	if (d < dmin_level[level]) dmin_level[level] = d;
	if (d > dmax_level[level]) dmax_level[level] = d;
	double n = std::sqrt(q);
	return {w.x / n, w.y / n, w.z / n};
}

static int L;
static void walk(V3 v0, V3 v1, V3 v2, int level) {
	V3 w2 = mid(v0, v1, level), w0 = mid(v1, v2, level), w1 = mid(v2, v0, level);
	if (level + 1 >= L) return;
	walk(v0, w2, w1, level + 1);
	walk(v1, w0, w2, level + 1);
	walk(v2, w1, w0, level + 1);
	walk(w0, w1, w2, level + 1);
}

int main(int argc, char** argv) {
	// 1. Exhaustive check of the exponent-halving square root around 4.
	int64_t lo = 0, hi = 0;
	const int64_t R = int64_t(1) << 29;
	for (int64_t d = 0; d <= R; ++d) {
		double q = from(bits(4.0) + d);
		if (sqrt_near4(q) != std::sqrt(q)) { hi = d; break; }
		hi = d + 1;
	}
	for (int64_t d = 0; d <= R; ++d) {
		double q = from(bits(4.0) - d);
		if (sqrt_near4(q) != std::sqrt(q)) { lo = d; break; }
		lo = d + 1;
	}
	std::printf("exact for D in (-%" PRId64 ", %" PRId64 ") (2^%.2f, 2^%.2f)\n", lo, hi, std::log2((double)lo), std::log2((double)hi));

	// 2. The range of D at each level of the mesh.
	L = argc > 1 ? std::atoi(argv[1]) : 10;
	for (int i = 0; i < 32; ++i) { dmin_level[i] = INT64_MAX; dmax_level[i] = INT64_MIN; }
	const V3 sv[6] = {{0,0,1},{1,0,0},{0,1,0},{-1,0,0},{0,-1,0},{0,0,-1}};
	const int st[8][3] = {{1,0,4},{4,0,3},{3,0,2},{2,0,1},{1,5,2},{2,5,3},{3,5,4},{4,5,1}};
	for (auto& t : st) walk(sv[t[0]], sv[t[1]], sv[t[2]], 0);
	for (int k = 0; k < L; ++k) {
		std::printf("depth %2d: D in [%" PRId64 ", %" PRId64 "]  |D| 2^%.2f\n", k, dmin_level[k], dmax_level[k],
			std::log2((double)std::max(-dmin_level[k], dmax_level[k] > 0 ? dmax_level[k] : 1)));
	}
}
