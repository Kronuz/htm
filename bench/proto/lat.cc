// Latency and throughput of double fdiv/fsqrt, scalar and 2-lane.
#include <chrono>
#include <cmath>
#include <cstdio>
#if defined(__aarch64__)
#define OPQ(v) __asm__ volatile("" : "+w"(v))
#else
#define OPQ(v) __asm__ volatile("" : "+x"(v))
#endif
#if defined(__aarch64__)
#include <arm_neon.h>
typedef float64x2_t d2;
static inline d2 vsq(d2 a) { return vsqrtq_f64(a); }
static inline d2 vdv(d2 a, d2 b) { return vdivq_f64(a, b); }
static inline d2 vset(double a) { return vdupq_n_f64(a); }
static inline double v0(d2 a) { return vgetq_lane_f64(a, 0); }
#else
#include <emmintrin.h>
typedef __m128d d2;
static inline d2 vsq(d2 a) { return _mm_sqrt_pd(a); }
static inline d2 vdv(d2 a, d2 b) { return _mm_div_pd(a, b); }
static inline d2 vset(double a) { return _mm_set1_pd(a); }
static inline double v0(d2 a) { return _mm_cvtsd_f64(a); }
#endif

template <class F>
static double time_it(const char* what, long iters, double ops_per_iter, F f) {
	double best = 1e30;
	for (int r = 0; r < 5; ++r) {
		auto t0 = std::chrono::steady_clock::now();
		double s = f(iters);
		auto t1 = std::chrono::steady_clock::now();
		double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / (iters * ops_per_iter);
		if (ns < best) best = ns;
		if (s == 12345.678) std::printf("x");
	}
	std::printf("%-28s %.3f ns/op\n", what, best);
	return best;
}

int main() {
	const long N = 20000000;
	volatile double seed = 1.0000001;
	double base = time_it("add latency", N, 1, [&](long n) { double a = seed; for (long i = 0; i < n; ++i) { a = a + 1e-300; OPQ(a); } return a; });
	(void)base;
	time_it("mul latency", N, 1, [&](long n) { double a = seed; for (long i = 0; i < n; ++i) { a = a * 1.0000000001; OPQ(a); } return a; });
	time_it("div latency", N, 1, [&](long n) { double a = seed; for (long i = 0; i < n; ++i) { a = a / 1.0000000001; OPQ(a); } return a; });
	time_it("sqrt latency", N, 1, [&](long n) { double a = seed + 3; for (long i = 0; i < n; ++i) { a = std::sqrt(a) + 2.0; OPQ(a); } return a; });
	time_it("div throughput (8 indep)", N / 8, 8, [&](long n) {
		double a[8]; for (int j = 0; j < 8; ++j) a[j] = seed + j;
		for (long i = 0; i < n; ++i) for (int j = 0; j < 8; ++j) { a[j] = a[j] / 1.0000000001; OPQ(a[j]); }
		return a[0] + a[7]; });
	time_it("sqrt throughput (8 indep)", N / 8, 8, [&](long n) {
		double a[8]; for (int j = 0; j < 8; ++j) a[j] = seed + 3 + j;
		for (long i = 0; i < n; ++i) for (int j = 0; j < 8; ++j) { a[j] = std::sqrt(a[j]); OPQ(a[j]); }
		return a[0] + a[7]; });
	time_it("vdiv latency", N, 1, [&](long n) { d2 a = vset(seed); d2 b = vset(1.0000000001); for (long i = 0; i < n; ++i) { a = vdv(a, b); OPQ(a); } return v0(a); });
	time_it("vsqrt latency", N, 1, [&](long n) { d2 a = vset(seed); for (long i = 0; i < n; ++i) { a = vsq(a); OPQ(a); } return v0(a); });
	time_it("vdiv throughput (8 indep)", N / 8, 8, [&](long n) {
		d2 a[8]; for (int j = 0; j < 8; ++j) a[j] = vset(seed + j); d2 b = vset(1.0000000001);
		for (long i = 0; i < n; ++i) for (int j = 0; j < 8; ++j) { a[j] = vdv(a[j], b); OPQ(a[j]); }
		return v0(a[0]) + v0(a[7]); });
	time_it("vsqrt throughput (8 indep)", N / 8, 8, [&](long n) {
		d2 a[8]; for (int j = 0; j < 8; ++j) a[j] = vset(seed + 3 + j);
		for (long i = 0; i < n; ++i) for (int j = 0; j < 8; ++j) { a[j] = vsq(a[j]); OPQ(a[j]); }
		return v0(a[0]) + v0(a[7]); });
}
