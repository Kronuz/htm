// The exponent-halving square root of the descent (sqrt_near4 in htm.cc) against
// std::sqrt, and the range of the mesh it is used on.
//
// 1. Every double q within 2^25 units in the last place of 4: halving the bits of q
//    and adding 0x1FF8000000000000 gives the bits of RN(sqrt(q)).
// 2. The sums of squares of the descent's midpoints (|fl(a + b)|^2 for an edge a-b of
//    a trixel at depth k): every trixel to depth 10, where their largest distance from
//    4 shrinks by at least 3.9 per level, and random paths to depth 25, where every one
//    from depth 15 on lies within 2^24 units of 4.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>

namespace {

uint64_t bits(double d) {
	uint64_t b;
	std::memcpy(&b, &d, sizeof b);
	return b;
}

double from_bits(uint64_t b) {
	double d;
	std::memcpy(&d, &b, sizeof d);
	return d;
}

double sqrt_near4(double q) {
	return from_bits((bits(q) >> 1) + 0x1FF8000000000000ULL);
}

constexpr int NEAR4_DEPTH = 15;  // htm.cc's HTM_NEAR4_DEPTH
constexpr int64_t EXACT_RANGE = int64_t(1) << 25;

volatile double sink;
double r(double v) {  // every operation rounded on its own, as in the descent
	sink = v;
	return sink;
}

struct V {
	double x, y, z;
};

int64_t max_d[32];

V mid(const V& a, const V& b, int depth) {
	double wx = r(a.x + b.x), wy = r(a.y + b.y), wz = r(a.z + b.z);
	double q = r(r(r(wx * wx) + r(wy * wy)) + r(wz * wz));
	int64_t d = std::llabs(static_cast<int64_t>(bits(q)) - static_cast<int64_t>(bits(4.0)));
	if (d > max_d[depth]) max_d[depth] = d;
	double n = r(std::sqrt(q));
	return {r(wx / n), r(wy / n), r(wz / n)};
}

void walk(const V& v0, const V& v1, const V& v2, int depth, int max_depth) {
	V w2 = mid(v0, v1, depth), w0 = mid(v1, v2, depth), w1 = mid(v2, v0, depth);
	if (depth + 1 >= max_depth) return;
	walk(v0, w2, w1, depth + 1, max_depth);
	walk(v1, w0, w2, depth + 1, max_depth);
	walk(v2, w1, w0, depth + 1, max_depth);
	walk(w0, w1, w2, depth + 1, max_depth);
}

}  // namespace

int main() {
	size_t failures = 0;

	// 1. The square root on every q within EXACT_RANGE units of 4.
	for (int64_t d = -EXACT_RANGE; d <= EXACT_RANGE; ++d) {
		double q = from_bits(bits(4.0) + d);
		if (sqrt_near4(q) != std::sqrt(q) && ++failures <= 10) {
			std::printf("sqrt_near4(%a) = %a, sqrt = %a\n", q, sqrt_near4(q), std::sqrt(q));
		}
	}
	std::printf("sqrt_near4: %lld values around 4, %zu wrong\n", (long long)(2 * EXACT_RANGE + 1), failures);

	// 2. Every trixel to depth 10.
	const V sv[6] = {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}, {-1, 0, 0}, {0, -1, 0}, {0, 0, -1}};
	const int st[8][3] = {{1, 0, 4}, {4, 0, 3}, {3, 0, 2}, {2, 0, 1}, {1, 5, 2}, {2, 5, 3}, {3, 5, 4}, {4, 5, 1}};
	const int full = 10;
	for (const auto& t : st) walk(sv[t[0]], sv[t[1]], sv[t[2]], 0, full);
	for (int k = 0; k < full; ++k) {
		std::printf("depth %2d: max |q - 4| = %lld units (2^%.2f)\n", k, (long long)max_d[k], std::log2(double(max_d[k])));
	}
	for (int k = 5; k < full; ++k) {
		if (!(max_d[k] * 39 <= max_d[k - 1] * 10) && ++failures <= 10) {
			std::printf("depth %d: max |q - 4| shrank by less than 3.9\n", k);
		}
	}
	// Shrinking by 3.9 per level from depth 9, depth NEAR4_DEPTH stays in range.
	double bound = max_d[full - 1];
	for (int k = full; k <= NEAR4_DEPTH; ++k) bound /= 3.9;
	std::printf("depth %d: max |q - 4| at most 2^%.2f units by that rate\n", NEAR4_DEPTH, std::log2(bound));
	if (!(bound < EXACT_RANGE / 2) && ++failures <= 10) std::printf("depth %d not within range\n", NEAR4_DEPTH);

	// Random paths to depth 25.
	std::mt19937_64 rng(1101);
	for (auto& m : max_d) m = 0;
	for (int i = 0; i < 100000; ++i) {
		const auto& t = st[rng() % 8];
		V v0 = sv[t[0]], v1 = sv[t[1]], v2 = sv[t[2]];
		for (int depth = 0; depth < 25; ++depth) {
			V w2 = mid(v0, v1, depth), w0 = mid(v1, v2, depth), w1 = mid(v2, v0, depth);
			switch (rng() % 4) {
				case 0: v1 = w2; v2 = w1; break;
				case 1: v0 = v1; v1 = w0; v2 = w2; break;
				case 2: v0 = v2; v1 = w1; v2 = w0; break;
				default: v0 = w0; v1 = w1; v2 = w2; break;
			}
		}
	}
	for (int k = NEAR4_DEPTH; k < 25; ++k) {
		if (!(max_d[k] < (int64_t(1) << 24)) && ++failures <= 10) {
			std::printf("depth %d: |q - 4| = %lld units on a random path\n", k, (long long)max_d[k]);
		}
	}
	std::printf("random paths: max |q - 4| at depth %d is 2^%.2f units\n", NEAR4_DEPTH, std::log2(double(max_d[NEAR4_DEPTH])));

	std::printf("failures %zu\n", failures);
	return failures ? 1 : 0;
}
