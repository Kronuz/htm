// Prototype: the old getId descent (the spec) against a predicted, verified descent.
#if defined(__aarch64__)
#include <arm_neon.h>
#elif defined(__SSE2__)
#include <emmintrin.h>
#endif
#include <cstring>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>
#include <algorithm>


struct Cartesian { double x, y, z; Cartesian(double a, double b, double c) : x(a), y(b), z(c) {}
	void normalize() { double n = std::sqrt(x * x + y * y + z * z); x /= n; y /= n; z /= n; } };
struct trixel_t { uint64_t id; int v0, v1, v2; };
struct SV { double x, y, z; };
static const SV start_vertices[6] = {{0,0,1},{1,0,0},{0,1,0},{-1,0,0},{0,-1,0},{0,0,-1}};
static const trixel_t start_trixels[8] = {{8,1,0,4},{9,4,0,3},{10,3,0,2},{11,2,0,1},{12,1,5,2},{13,2,5,3},{14,3,5,4},{15,4,5,1}};
namespace HTM {
inline const trixel_t& startTrixel(const Cartesian& coord) {
	int num;
	if (coord.x > 0 && coord.y >= 0) num = (coord.z >= 0) ? 3 : 4;
	else if (coord.x <= 0 && coord.y > 0) num = (coord.z >= 0) ? 2 : 5;
	else if (coord.x < 0 && coord.y <= 0) num = (coord.z >= 0) ? 1 : 6;
	else if (coord.x >= 0 && coord.y < 0) num = (coord.z >= 0) ? 0 : 7;
	else num = (coord.z >= 0) ? 3 : 4;
	return start_trixels[num];
}
}

#pragma clang fp contract(off)
#ifndef KI
#define KI 15
#endif
#ifndef KX
#define KX 21
#endif

namespace {

struct V3 { double x, y, z; };

inline V3 mid(const V3& a, const V3& b) {
	V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
	double n = std::sqrt(w.x * w.x + w.y * w.y + w.z * w.z);
	return {w.x / n, w.y / n, w.z / n};
}


// RN(sqrt(q)) for q within 2^26 ulps of 4: halve the exponent bits.
inline double sqrt_near4(double q) {
#if defined(__aarch64__)
	uint64x1_t b = vreinterpret_u64_f64(vdup_n_f64(q));
	b = vadd_u64(vshr_n_u64(b, 1), vdup_n_u64(0x1FF8000000000000ULL));
	return vget_lane_f64(vreinterpret_f64_u64(b), 0);
#elif defined(__SSE2__)
	__m128i b = _mm_castpd_si128(_mm_set_sd(q));
	b = _mm_add_epi64(_mm_srli_epi64(b, 1), _mm_set1_epi64x(0x1FF8000000000000LL));
	return _mm_cvtsd_f64(_mm_castsi128_pd(b));
#else
	uint64_t b; std::memcpy(&b, &q, 8); b = (b >> 1) + 0x1FF8000000000000ULL; std::memcpy(&q, &b, 8); return q;
#endif
}

inline V3 mid4(const V3& a, const V3& b) {
	V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
	double n = sqrt_near4(w.x * w.x + w.y * w.y + w.z * w.z);
	return {w.x / n, w.y / n, w.z / n};
}

inline double tri(const V3& a, const V3& b, const V3& c) {
	double cx = a.y * b.z - b.y * a.z;
	double cy = a.z * b.x - b.z * a.x;
	double cz = a.x * b.y - b.x * a.y;
	return cx * c.x + cy * c.y + cz * c.z;
}

inline bool inside(const V3& v, const V3& v0, const V3& v1, const V3& v2) {
	return tri(v0, v1, v) > 0 && tri(v1, v2, v) > 0 && tri(v2, v0, v) > 0;
}

__attribute__((noinline)) uint64_t old_descend(const Cartesian& coord) {
	const auto& st = HTM::startTrixel(coord);
	const auto& s0 = start_vertices[st.v0];
	const auto& s1 = start_vertices[st.v1];
	const auto& s2 = start_vertices[st.v2];
	V3 v0{s0.x, s0.y, s0.z}, v1{s1.x, s1.y, s1.z}, v2{s2.x, s2.y, s2.z};
	const V3 c{coord.x, coord.y, coord.z};
	auto id = st.id;
	for (int depth = 0; depth < 25; ++depth) {
		auto w2 = mid(v0, v1);
		auto w0 = mid(v1, v2);
		auto w1 = mid(v2, v0);
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

size_t slow_count = 0;

inline V3 sel(int p, const V3& a, const V3& b, const V3& c, const V3& d) {
	return p == 0 ? a : p == 1 ? b : p == 2 ? c : d;
}

template <int KINT>
__attribute__((noinline)) uint64_t new_descend(const Cartesian& coord) {
	const auto& st = HTM::startTrixel(coord);
	const auto& s0 = start_vertices[st.v0];
	const auto& s1 = start_vertices[st.v1];
	const auto& s2 = start_vertices[st.v2];
	V3 v0{s0.x, s0.y, s0.z}, v1{s1.x, s1.y, s1.z}, v2{s2.x, s2.y, s2.z};
	const V3 c{coord.x, coord.y, coord.z};
	uint64_t id = st.id;
	double e0 = tri(v1, v2, c), e1 = tri(v2, v0, c), e2 = tri(v0, v1, c);
	for (int depth = 0; depth < 25; ++depth) {
		V3 w0, w1, w2;
		if (depth < KINT) { w0 = mid(v1, v2); w1 = mid(v2, v0); w2 = mid(v0, v1); }
		else { w0 = mid4(v1, v2); w1 = mid4(v2, v0); w2 = mid4(v0, v1); }
		double s = e0 + e1 + e2;
		int p = (e0 + e0 > s) ? 0 : (e1 + e1 > s) ? 1 : (e2 + e2 > s) ? 2 : 3;
		V3 n0 = sel(p, v0, v1, v2, w0);
		V3 n1 = sel(p, w2, w0, w1, w1);
		V3 n2 = sel(p, w1, w2, w0, w2);
		double f0 = tri(n1, n2, c), f1 = tri(n2, n0, c), f2 = tri(n0, n1, c);
		double ia = tri(w1, w2, c), ib = tri(w2, w0, c);
		bool ok = f0 > 0 && f1 > 0 && f2 > 0 && (p != 1 || ia > 0) && (p != 2 || (ia > 0 && ib > 0));
		if (!ok) [[unlikely]] {
			++slow_count;
			if (inside(c, v0, w2, w1)) p = 0;
			else if (inside(c, v1, w0, w2)) p = 1;
			else if (inside(c, v2, w1, w0)) p = 2;
			else p = 3;
			n0 = sel(p, v0, v1, v2, w0);
			n1 = sel(p, w2, w0, w1, w1);
			n2 = sel(p, w1, w2, w0, w2);
			f0 = tri(n1, n2, c); f1 = tri(n2, n0, c); f2 = tri(n0, n1, c);
		}
		id = (id << 2) | p;
		v0 = n0; v1 = n1; v2 = n2;
		e0 = f0; e1 = f1; e2 = f2;
	}
	return id;
}



typedef double d2 __attribute__((vector_size(16)));
typedef uint64_t u2 __attribute__((vector_size(16)));

inline d2 vsqrt2(d2 q) {
#if defined(__aarch64__)
	return vsqrtq_f64(q);
#elif defined(__SSE2__)
	return _mm_sqrt_pd(q);
#else
	return d2{std::sqrt(q[0]), std::sqrt(q[1])};
#endif
}

inline d2 vsqrt2_near4(d2 q) {
	u2 b = (u2)q;
	b = (b >> 1) + 0x1FF8000000000000ULL;
	return (d2)b;
}

// w0 = mid(v1, v2) and w1 = mid(v2, v0) in the two lanes, w2 = mid(v0, v1) on its own.
template <bool Near4>
inline void mids(const V3& v0, const V3& v1, const V3& v2, V3& w0, V3& w1, V3& w2) {
	d2 sx = d2{v1.x, v2.x} + d2{v2.x, v0.x};
	d2 sy = d2{v1.y, v2.y} + d2{v2.y, v0.y};
	d2 sz = d2{v1.z, v2.z} + d2{v2.z, v0.z};
	d2 q = sx * sx + sy * sy + sz * sz;
	d2 n = Near4 ? vsqrt2_near4(q) : vsqrt2(q);
	d2 wx = sx / n, wy = sy / n, wz = sz / n;
	w0 = {wx[0], wy[0], wz[0]};
	w1 = {wx[1], wy[1], wz[1]};
	w2 = Near4 ? mid4(v0, v1) : mid(v0, v1);
}


// v5: all three midpoints with two-lane divisions: (w0, w1) per component, w2's x and y
// together, w2's z alone.
template <bool Near4>
inline void mids5(const V3& v0, const V3& v1, const V3& v2, V3& w0, V3& w1, V3& w2) {
	d2 sx = d2{v1.x, v2.x} + d2{v2.x, v0.x};
	d2 sy = d2{v1.y, v2.y} + d2{v2.y, v0.y};
	d2 sz = d2{v1.z, v2.z} + d2{v2.z, v0.z};
	d2 q = sx * sx + sy * sy + sz * sz;
	d2 txy = d2{v0.x, v0.y} + d2{v1.x, v1.y};
	double tz = v0.z + v1.z;
	d2 t2 = txy * txy;
	double q2 = (t2[0] + t2[1]) + tz * tz;
	d2 n = Near4 ? vsqrt2_near4(q) : vsqrt2(q);
	double n2 = Near4 ? sqrt_near4(q2) : std::sqrt(q2);
	d2 wx = sx / n, wy = sy / n, wz = sz / n;
	d2 wxy = txy / d2{n2, n2};
	w0 = {wx[0], wy[0], wz[0]};
	w1 = {wx[1], wy[1], wz[1]};
	w2 = {wxy[0], wxy[1], tz / n2};
}

size_t slow2 = 0, extra2 = 0;

// Predicted descent with exact verification; the inner tests that rule out the
// earlier children are certified from the parent's exact edge tests (margin 2^-44 C)
// above depth KX and computed exactly from KX on.
template <int KINT, int KXL, int SIMD = 0>
__attribute__((noinline)) uint64_t new_descend2(const Cartesian& coord) {
	const V3 c{coord.x, coord.y, coord.z};
	double C = std::fmax(std::fabs(c.x), std::fmax(std::fabs(c.y), std::fabs(c.z)));
	if (!(C >= 0x1p-500 && C <= 0x1p500)) return old_descend(coord);
	const double tauC = C * 0x1p-44;
	const auto& st = HTM::startTrixel(coord);
	const auto& s0 = start_vertices[st.v0];
	const auto& s1 = start_vertices[st.v1];
	const auto& s2 = start_vertices[st.v2];
	V3 v0{s0.x, s0.y, s0.z}, v1{s1.x, s1.y, s1.z}, v2{s2.x, s2.y, s2.z};
	uint64_t id = st.id;
	double e0 = tri(v1, v2, c), e1 = tri(v2, v0, c), e2 = tri(v0, v1, c);
	for (int depth = 0; depth < 25; ++depth) {
		V3 w0, w1, w2;
		if (SIMD == 2) { if (depth < KINT) mids5<false>(v0, v1, v2, w0, w1, w2); else mids5<true>(v0, v1, v2, w0, w1, w2); }
		else if (SIMD) { if (depth < KINT) mids<false>(v0, v1, v2, w0, w1, w2); else mids<true>(v0, v1, v2, w0, w1, w2); }
		else if (depth < KINT) { w0 = mid(v1, v2); w1 = mid(v2, v0); w2 = mid(v0, v1); }
		else { w0 = mid4(v1, v2); w1 = mid4(v2, v0); w2 = mid4(v0, v1); }
		double fa = (e1 + e2) - e0, fb = (e0 + e2) - e1, fc = (e0 + e1) - e2;
		int p = fa < 0 ? 0 : fb < 0 ? 1 : fc < 0 ? 2 : 3;
		V3 n0 = sel(p, v0, v1, v2, w0);
		V3 n1 = sel(p, w2, w0, w1, w1);
		V3 n2 = sel(p, w1, w2, w0, w2);
		double f0 = tri(n1, n2, c), f1 = tri(n2, n0, c), f2 = tri(n0, n1, c);
		bool ok = f0 > 0 && f1 > 0 && f2 > 0;
		if (depth < KXL) {
			bool need_a = (p == 1 || p == 2) && !(fa > tauC);
			bool need_b = p == 2 && !(fb > tauC);
			if (need_a || need_b) [[unlikely]] {
				++extra2;
				if (need_a) ok = ok && tri(w1, w2, c) > 0;
				if (need_b) ok = ok && tri(w2, w0, c) > 0;
			}
		} else {
			double ia = tri(w1, w2, c), ib = tri(w2, w0, c);
			ok = ok && ((p != 1 && p != 2) || ia > 0) && (p != 2 || ib > 0);
		}
		if (!ok) [[unlikely]] {
			++slow2;
			if (inside(c, v0, w2, w1)) p = 0;
			else if (inside(c, v1, w0, w2)) p = 1;
			else if (inside(c, v2, w1, w0)) p = 2;
			else p = 3;
			n0 = sel(p, v0, v1, v2, w0);
			n1 = sel(p, w2, w0, w1, w1);
			n2 = sel(p, w1, w2, w0, w2);
			f0 = tri(n1, n2, c); f1 = tri(n2, n0, c); f2 = tri(n0, n1, c);
		}
		id = (id << 2) | p;
		v0 = n0; v1 = n1; v2 = n2;
		e0 = f0; e1 = f1; e2 = f2;
	}
	return id;
}

// v4: the sums, norms and square roots of all three edges, then only the divisions
// of the child's new vertices (the third only for child 3).
template <bool Near4>
inline void sums(const V3& a, const V3& b, V3& s, double& n) {
	s = {a.x + b.x, a.y + b.y, a.z + b.z};
	double q = s.x * s.x + s.y * s.y + s.z * s.z;
	n = Near4 ? sqrt_near4(q) : std::sqrt(q);
}
inline V3 dv(const V3& s, double n) { return {s.x / n, s.y / n, s.z / n}; }
inline double pk(int p, double a, double b, double c, double d) { return p == 0 ? a : p == 1 ? b : p == 2 ? c : d; }

size_t slow4 = 0;
template <int KINT, int KXL>
__attribute__((noinline)) uint64_t new_descend4(const Cartesian& coord) {
	const V3 c{coord.x, coord.y, coord.z};
	double C = std::fmax(std::fabs(c.x), std::fmax(std::fabs(c.y), std::fabs(c.z)));
	if (!(C >= 0x1p-500 && C <= 0x1p500)) return old_descend(coord);
	const double tauC = C * 0x1p-44;
	const auto& st = HTM::startTrixel(coord);
	const auto& s0 = start_vertices[st.v0];
	const auto& s1 = start_vertices[st.v1];
	const auto& s2 = start_vertices[st.v2];
	V3 v0{s0.x, s0.y, s0.z}, v1{s1.x, s1.y, s1.z}, v2{s2.x, s2.y, s2.z};
	uint64_t id = st.id;
	double e0 = tri(v1, v2, c), e1 = tri(v2, v0, c), e2 = tri(v0, v1, c);
	for (int depth = 0; depth < 25; ++depth) {
		double fa = (e1 + e2) - e0, fb = (e0 + e2) - e1, fc = (e0 + e1) - e2;
		int p = fa < 0 ? 0 : fb < 0 ? 1 : fc < 0 ? 2 : 3;
		V3 n0, n1, n2;
		bool ok;
		double f0, f1, f2;
		if (depth < KXL) {
			V3 S0, S1, S2; double N0, N1, N2;
			if (depth < KINT) { sums<false>(v1, v2, S0, N0); sums<false>(v2, v0, S1, N1); sums<false>(v0, v1, S2, N2); }
			else { sums<true>(v1, v2, S0, N0); sums<true>(v2, v0, S1, N1); sums<true>(v0, v1, S2, N2); }
			V3 SA = sel(p, S2, S0, S1, S1), SB = sel(p, S1, S2, S0, S2);
			double NA = pk(p, N2, N0, N1, N1), NB = pk(p, N1, N2, N0, N2);
			n1 = dv(SA, NA); n2 = dv(SB, NB);
			if (p == 3) n0 = dv(S0, N0); else n0 = sel(p, v0, v1, v2, v0);
			f0 = tri(n1, n2, c); f1 = tri(n2, n0, c); f2 = tri(n0, n1, c);
			ok = f0 > 0 && f1 > 0 && f2 > 0;
			bool need_a = (p == 1 || p == 2) && !(fa > tauC);
			bool need_b = p == 2 && !(fb > tauC);
			if (need_a || need_b) [[unlikely]] ok = false;
		} else {
			V3 w0 = mid4(v1, v2), w1 = mid4(v2, v0), w2 = mid4(v0, v1);
			n0 = sel(p, v0, v1, v2, w0); n1 = sel(p, w2, w0, w1, w1); n2 = sel(p, w1, w2, w0, w2);
			f0 = tri(n1, n2, c); f1 = tri(n2, n0, c); f2 = tri(n0, n1, c);
			double ia = tri(w1, w2, c), ib = tri(w2, w0, c);
			ok = f0 > 0 && f1 > 0 && f2 > 0 && ((p != 1 && p != 2) || ia > 0) && (p != 2 || (ia > 0 && ib > 0));
		}
		if (!ok) [[unlikely]] {
			++slow4;
			V3 w0 = mid(v1, v2), w1 = mid(v2, v0), w2 = mid(v0, v1);
			if (inside(c, v0, w2, w1)) p = 0;
			else if (inside(c, v1, w0, w2)) p = 1;
			else if (inside(c, v2, w1, w0)) p = 2;
			else p = 3;
			n0 = sel(p, v0, v1, v2, w0); n1 = sel(p, w2, w0, w1, w1); n2 = sel(p, w1, w2, w0, w2);
			f0 = tri(n1, n2, c); f1 = tri(n2, n0, c); f2 = tri(n0, n1, c);
		}
		id = (id << 2) | p;
		v0 = n0; v1 = n1; v2 = n2;
		e0 = f0; e1 = f1; e2 = f2;
	}
	return id;
}

// v6: the next child is predicted from the parent's exact edge tests and the norms of
// the midpoint sums, before its vertices exist, so a level computes only the midpoints
// of its child (two, three for child 3).
size_t slow6 = 0;
template <bool Near4>
inline V3 mid_n(const V3& a, const V3& b, double& n) {
	V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
	double q = w.x * w.x + w.y * w.y + w.z * w.z;
	n = Near4 ? sqrt_near4(q) : std::sqrt(q);
	return {w.x / n, w.y / n, w.z / n};
}
inline int predict(double e0, double e1, double e2) {
	double fa = (e1 + e2) - e0, fb = (e0 + e2) - e1, fc = (e0 + e1) - e2;
	return fa < 0 ? 0 : fb < 0 ? 1 : fc < 0 ? 2 : 3;
}
template <int KINT, int KXL>
__attribute__((noinline)) uint64_t new_descend6(const Cartesian& coord) {
	const V3 c{coord.x, coord.y, coord.z};
	double C = std::fmax(std::fabs(c.x), std::fmax(std::fabs(c.y), std::fabs(c.z)));
	if (!(C >= 0x1p-500 && C <= 0x1p500)) return old_descend(coord);
	const double tauC = C * 0x1p-44;
	const auto& st = HTM::startTrixel(coord);
	const auto& s0 = start_vertices[st.v0];
	const auto& s1 = start_vertices[st.v1];
	const auto& s2 = start_vertices[st.v2];
	V3 v0{s0.x, s0.y, s0.z}, v1{s1.x, s1.y, s1.z}, v2{s2.x, s2.y, s2.z};
	uint64_t id = st.id;
	double e0 = tri(v1, v2, c), e1 = tri(v2, v0, c), e2 = tri(v0, v1, c);
	int p = predict(e0, e1, e2);
	for (int depth = 0; depth < 25; ++depth) {
		// The two new vertices of child p: n1 = mid(a1, b1), n2 = mid(a2, b2).
		const V3& a1 = sel(p, v0, v1, v2, v2);  // w2 = mid(v0,v1), w0 = mid(v1,v2), w1 = mid(v2,v0), w1
		const V3& b1 = sel(p, v1, v2, v0, v0);
		const V3& a2 = sel(p, v2, v0, v1, v0);  // w1 = mid(v2,v0), w2 = mid(v0,v1), w0 = mid(v1,v2), w2
		const V3& b2 = sel(p, v0, v1, v2, v1);
		double na, nb, nc = 0;
		V3 n0, n1, n2;
		if (depth < KINT) { n1 = mid_n<false>(a1, b1, na); n2 = mid_n<false>(a2, b2, nb); }
		else { n1 = mid_n<true>(a1, b1, na); n2 = mid_n<true>(a2, b2, nb); }
		if (p == 3) n0 = depth < KINT ? mid_n<false>(v1, v2, nc) : mid_n<true>(v1, v2, nc);
		else n0 = sel(p, v0, v1, v2, v0);
		// The child's tests predicted from the parent's, over positive factors.
		double g0, g1, g2;
		switch (p) {
			case 0: g0 = (e0 - e1) - e2; g1 = e1 * nb; g2 = e2 * na; break;   // na = |v0+v1|, nb = |v2+v0|
			case 1: g0 = (e1 - e0) - e2; g1 = e2 * nb; g2 = e0 * na; break;   // na = |v1+v2|, nb = |v0+v1|
			case 2: g0 = (e2 - e0) - e1; g1 = e0 * nb; g2 = e1 * na; break;   // na = |v2+v0|, nb = |v1+v2|
			default: g0 = ((e1 + e2) - e0) * nc; g1 = ((e0 + e2) - e1) * na; g2 = ((e0 + e1) - e2) * nb; break;
		}
		int pn = predict(g0, g1, g2);
		double f0 = tri(n1, n2, c), f1 = tri(n2, n0, c), f2 = tri(n0, n1, c);
		bool ok = f0 > 0 && f1 > 0 && f2 > 0;
		if (depth < KXL) {
			double fa = (e1 + e2) - e0, fb = (e0 + e2) - e1;
			if (((p == 1 || p == 2) && !(fa > tauC)) || (p == 2 && !(fb > tauC))) ok = false;
		} else if (p == 1 || p == 2) {
			V3 w0 = mid4(v1, v2), w1 = mid4(v2, v0), w2 = mid4(v0, v1);
			ok = ok && tri(w1, w2, c) > 0 && (p != 2 || tri(w2, w0, c) > 0);
		}
		if (!ok) [[unlikely]] {
			++slow6;
			V3 w0 = mid(v1, v2), w1 = mid(v2, v0), w2 = mid(v0, v1);
			if (inside(c, v0, w2, w1)) p = 0;
			else if (inside(c, v1, w0, w2)) p = 1;
			else if (inside(c, v2, w1, w0)) p = 2;
			else p = 3;
			n0 = sel(p, v0, v1, v2, w0); n1 = sel(p, w2, w0, w1, w1); n2 = sel(p, w1, w2, w0, w2);
			f0 = tri(n1, n2, c); f1 = tri(n2, n0, c); f2 = tri(n0, n1, c);
			pn = predict(f0, f1, f2);
		}
		id = (id << 2) | p;
		v0 = n0; v1 = n1; v2 = n2;
		e0 = f0; e1 = f1; e2 = f2;
		p = pn;
	}
	return id;
}

}  // namespace

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 1000000;
	std::mt19937_64 rng(42);
	std::normal_distribution<double> g;
	std::vector<Cartesian> pts;
	for (size_t i = 0; i < n; ++i) {
		double x = g(rng), y = g(rng), z = g(rng);
		pts.emplace_back(x, y, z);
		pts.back().normalize();
	}
	size_t diffs = 0, diffs4 = 0;
	for (auto& p : pts) diffs += old_descend(p) != new_descend<99>(p);
	size_t sc = slow_count;
	for (auto& p : pts) diffs4 += old_descend(p) != new_descend<KI>(p);
	size_t diffs5 = 0, diffs6 = 0;
	for (auto& p : pts) diffs5 += old_descend(p) != new_descend2<99, KX>(p);
	size_t sl2 = slow2, ex2 = extra2;
	for (auto& p : pts) diffs6 += old_descend(p) != new_descend2<KI, KX>(p);
	size_t diffs10 = 0;
	for (auto& p : pts) diffs10 += old_descend(p) != new_descend6<KI, KX>(p);
	std::printf("v6 diffs %zu slow %.4f per point\n", diffs10, double(slow6) / n);
	if (diffs10) return 1;
	size_t diffs9 = 0;
	for (auto& p : pts) diffs9 += old_descend(p) != new_descend2<KI, KX, 2>(p);
	std::printf("v5 diffs %zu\n", diffs9);
	if (diffs9) return 1;
	size_t diffs8 = 0;
	for (auto& p : pts) diffs8 += old_descend(p) != new_descend4<KI, KX>(p);
	std::printf("v4 diffs %zu slow %.4f per point\n", diffs8, double(slow4) / n);
	if (diffs8) return 1;
	size_t diffs7 = 0;
	for (auto& p : pts) diffs7 += old_descend(p) != new_descend2<KI, KX, 1>(p);
	std::printf("v3 simd diffs %zu\n", diffs7);
	if (diffs7) return 1;
	std::printf("v2: diffs %zu isqrt-diffs %zu slow %.4f extra %.4f per point\n", diffs5, diffs6, double(sl2) / n, double(ex2) / n);
	if (diffs5 || diffs6) return 1;
	std::printf("points %zu diffs %zu isqrt-diffs %zu slow levels %zu (%.4f per point)\n", n, diffs, diffs4, sc, double(sc) / n);
	size_t m = std::min<size_t>(n, 20000);
	for (int r = 0; r < 5; ++r) {
		auto t0 = std::chrono::steady_clock::now();
		uint64_t s = 0;
		for (size_t i = 0; i < m; ++i) s += old_descend(pts[i]);
		auto t1 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend<99>(pts[i]);
		auto t2 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend<KI>(pts[i]);
		auto t3 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend2<99, KX>(pts[i]);
		auto t4 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend2<KI, KX>(pts[i]);
		auto t5 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend2<KI, KX, 1>(pts[i]);
		auto t6 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend4<KI, KX>(pts[i]);
		auto t7 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend2<KI, KX, 2>(pts[i]);
		auto t8 = std::chrono::steady_clock::now();
		for (size_t i = 0; i < m; ++i) s += new_descend6<KI, KX>(pts[i]);
		auto t9 = std::chrono::steady_clock::now();
		std::printf("old %.1f  new %.1f  new+isqrt %.1f  v2 %.1f  v2+isqrt %.1f  v3simd %.1f  v4 %.1f  v5 %.1f  v6 %.1f  (%llu)\n",
			std::chrono::duration<double, std::nano>(t1 - t0).count() / m,
			std::chrono::duration<double, std::nano>(t2 - t1).count() / m,
			std::chrono::duration<double, std::nano>(t3 - t2).count() / m,
			std::chrono::duration<double, std::nano>(t4 - t3).count() / m,
			std::chrono::duration<double, std::nano>(t5 - t4).count() / m,
			std::chrono::duration<double, std::nano>(t6 - t5).count() / m,
			std::chrono::duration<double, std::nano>(t7 - t6).count() / m,
			std::chrono::duration<double, std::nano>(t8 - t7).count() / m,
			std::chrono::duration<double, std::nano>(t9 - t8).count() / m, (unsigned long long)s);
	}
	return diffs != 0;
}
