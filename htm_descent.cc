// getId(Cartesian): the point-to-id descent, its plain and rounded forms and the probes that choose one.
/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "htm.h"

#include <algorithm>                              // for std::min
#include <cmath>                                  // for std::sqrt


namespace {

// Plain 3-vector for the hot descent. Every operation repeats the floating-point
// expression of the matching Cartesian operator (operator+, operator^, operator*,
// normalize()), in the same order.
//
// The arithmetic is plain IEEE-754 double: every product is rounded before it is
// added or subtracted. A compiler that contracts a * b + c into one fused
// multiply-add (the default for GCC, and for clang within one expression on any
// CPU with FMA) rounds once instead of twice, and a point within rounding of a
// trixel edge then lands in another trixel. Ids are stored in indexes, so they
// must not depend on that choice.
//
// Where the CPU has no fused multiply-add (x86-64 without FMA) nothing can fuse
// and the descent is the plain expressions. Elsewhere it is compiled twice. The
// plain form tells the compiler not to contract: clang's `fp contract(off)`
// pragma, GCC's `optimize` attribute, MSVC's `fp_contract` pragma. Clang ignores
// its pragma under an explicit -ffp-contract=fast, and a compiler may not honor
// the others, so the first call runs the plain form on 48 points whose ids a
// fusing build gets wrong; if any differs, getId uses the second form, whose
// products are rounded behind an empty asm that the optimizer cannot see through.
#if defined(__SSE2_MATH__) && (defined(__x86_64__) || defined(__i386__)) && !defined(__FMA__) && !defined(__FMA4__)
#define HTM_FUSABLE 0
#else
#define HTM_FUSABLE 1
#endif

#if !HTM_FUSABLE
#define HTM_NOFUSE_FN
#define HTM_NOFUSE_BODY
#elif defined(__clang__)
#define HTM_NOFUSE_FN
#define HTM_NOFUSE_BODY _Pragma("clang fp contract(off)")
#elif defined(__GNUC__)
#define HTM_NOFUSE_FN __attribute__((optimize("-ffp-contract=off")))
#define HTM_NOFUSE_BODY
#elif defined(_MSC_VER)
#pragma fp_contract(off)
#define HTM_NOFUSE_FN
#define HTM_NOFUSE_BODY
#else
#define HTM_NOFUSE_FN
#define HTM_NOFUSE_BODY
#endif


struct V3 {
	double x, y, z;
};


#if HTM_FUSABLE

inline double rounded_mul(double a, double b) noexcept {
	double p = a * b;
#if defined(__GNUC__) && defined(__aarch64__)
	__asm__("" : "+w"(p));
#elif defined(__GNUC__) && defined(__SSE2_MATH__)
	__asm__("" : "+x"(p));
#elif defined(__GNUC__)
	__asm__("" : "+m"(p));
#else
	volatile double t = p;
	p = t;
#endif
	return p;
}

// Round selects the asm form; the plain form is a * b under the no-contract setting.
template <bool Round>
HTM_NOFUSE_FN inline double mul(double a, double b) noexcept {
	HTM_NOFUSE_BODY
	if constexpr (Round) {
		return rounded_mul(a, b);
	} else {
		return a * b;
	}
}

#define HTM_MUL(R, a, b) mul<R>(a, b)

#else

#define HTM_MUL(R, a, b) ((a) * (b))

#endif


template <bool Round>
HTM_NOFUSE_FN inline V3 midpoint(const V3& a, const V3& b) noexcept {
	HTM_NOFUSE_BODY
	V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
	double n = std::sqrt(HTM_MUL(Round, w.x, w.x) + HTM_MUL(Round, w.y, w.y) + HTM_MUL(Round, w.z, w.z));
	return {w.x / n, w.y / n, w.z / n};
}


// (a ^ b) * c, as in HTM::insideVertex_Trixel's building block.
template <bool Round>
HTM_NOFUSE_FN inline double triple(const V3& a, const V3& b, const V3& c) noexcept {
	HTM_NOFUSE_BODY
	double cx = HTM_MUL(Round, a.y, b.z) - HTM_MUL(Round, b.y, a.z);
	double cy = HTM_MUL(Round, a.z, b.x) - HTM_MUL(Round, b.z, a.x);
	double cz = HTM_MUL(Round, a.x, b.y) - HTM_MUL(Round, b.x, a.y);
	return HTM_MUL(Round, cx, c.x) + HTM_MUL(Round, cy, c.y) + HTM_MUL(Round, cz, c.z);
}


// The child whose closed triangle a point is least outside of, the first on a tie:
// the descent's choice when rounding leaves the point in no child.
template <bool Round>
HTM_NOFUSE_FN inline int nearest(const V3& c, const V3& v0, const V3& v1, const V3& v2, const V3& w0, const V3& w1, const V3& w2, double t0, double t1, double t2) noexcept {
	HTM_NOFUSE_BODY
	const double m[4] = {
		std::min({triple<Round>(v0, w2, c), t0, triple<Round>(w1, v0, c)}),
		std::min({triple<Round>(v1, w0, c), t1, triple<Round>(w2, v1, c)}),
		std::min({triple<Round>(v2, w1, c), t2, triple<Round>(w0, v2, c)}),
		std::min({-t2, -t0, -t1}),
	};
	int best = 0;
	for (int k = 1; k < 4; ++k) {
		if (m[k] > m[best]) {
			best = k;
		}
	}
	return best;
}


template <bool Round>
HTM_NOFUSE_FN uint64_t descend(const Cartesian& coord) noexcept {
	HTM_NOFUSE_BODY
	const auto& start_trixel = HTM::startTrixel(coord);
	const auto& s0 = start_vertices[start_trixel.v0];
	const auto& s1 = start_vertices[start_trixel.v1];
	const auto& s2 = start_vertices[start_trixel.v2];
	V3 v0{s0.x, s0.y, s0.z};
	V3 v1{s1.x, s1.y, s1.z};
	V3 v2{s2.x, s2.y, s2.z};
	const V3 c{coord.x, coord.y, coord.z};
	auto id = start_trixel.id;

	// Children 0..2 are tested in order with closed triangles, so a point on an edge goes
	// to the first that holds it; child 3's edges are the exact negations of their inner
	// edges t0..t2. A point that rounding leaves in no child goes to the nearest one.
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = midpoint<Round>(v0, v1);
		auto w0 = midpoint<Round>(v1, v2);
		auto w1 = midpoint<Round>(v2, v0);
		double t0 = triple<Round>(w2, w1, c);
		double t1 = triple<Round>(w0, w2, c);
		double t2 = triple<Round>(w1, w0, c);
		int child;
		if (t0 >= 0 && triple<Round>(v0, w2, c) >= 0 && triple<Round>(w1, v0, c) >= 0) {
			child = 0;
		} else if (t1 >= 0 && triple<Round>(v1, w0, c) >= 0 && triple<Round>(w2, v1, c) >= 0) {
			child = 1;
		} else if (t2 >= 0 && triple<Round>(v2, w1, c) >= 0 && triple<Round>(w0, v2, c) >= 0) {
			child = 2;
		} else if (t0 <= 0 && t1 <= 0 && t2 <= 0) {
			child = 3;
		} else {
			child = nearest<Round>(c, v0, v1, v2, w0, w1, w2, t0, t1, t2);
		}
		id = (id << 2) + child;
		switch (child) {
			case 0:
				v1 = w2;
				v2 = w1;
				break;
			case 1:
				v0 = v1;
				v1 = w0;
				v2 = w2;
				break;
			case 2:
				v0 = v2;
				v1 = w1;
				v2 = w0;
				break;
			default:
				v0 = w0;
				v1 = w1;
				v2 = w2;
				break;
		}
	}

	return id;
}

#undef HTM_MUL

#if HTM_FUSABLE

// Points and the ids of IEEE-754 arithmetic (test/golden.h has more). A build that
// fuses gets some of them wrong.
struct probe_t {
	double x, y, z;
	uint64_t id;
};

const probe_t probes[] = {
	{0x1.7143929ab78c7p-2, 0x1.9994a0e5273f7p-2, 0x1.af695eb632132p-1, 12881359809030654ULL},
	{-0x1.6cc5e93b0fc1ap-2, -0x1.a84422bf27e89p-2, 0x1.accf90d8ebafdp-1, 10594081174125255ULL},
	{-0x1.631dc7ed6cf1bp-1, 0x1.0a9c416314d73p-1, 0x1.fdba02043df9ap-2, 12328104190415457ULL},
	{-0x1.922d5eb73ca42p-3, 0x1.4d2eb767b8bb9p-3, -0x1.ef1288d89ea8ap-1, 14971743887750959ULL},
	{0x1.e24a85ff2af6cp-2, -0x1.4ddfb122ffc1fp-2, 0x1.a3ac201d72806p-1, 9466734108671887ULL},
	{0x1.90bc362868237p-4, 0x1.d1f5e6b05aa85p-1, 0x1.9c62d501f1376p-2, 12504775144506823ULL},
	{-0x1.27086062ad855p-2, -0x1.c73aec5dedd3dp-1, -0x1.6c2256778b33dp-2, 16542991213780926ULL},
	{-0x1.98f85900936ccp-1, -0x1.a774271765c69p-2, 0x1.bf7a6bfb9266fp-2, 11223956368872413ULL},
	{0x1.85e3d16c6a0bap-1, 0x1.2747b70964e6ep-1, -0x1.2eea71ee71587p-2, 14454870337873790ULL},
	{0x1.f252c0c4e9f8ep-2, -0x1.81d6f763ab2f8p-1, -0x1.c481497e203a9p-2, 17947105761604125ULL},
	{0x1.37a5abb8c22ap-1, 0x1.82d277f5243e7p-2, 0x1.653a7fd168122p-1, 13277068919624078ULL},
	{0x1.aa216a26fe8dfp-2, 0x1.7f9b83b947addp-2, -0x1.a837160fe4513p-1, 13968848208776054ULL},
	{-0x1.fda1b2c7fac9cp-3, -0x1.17c8c431e2bcep-1, 0x1.996dca9a3f4fep-1, 10595698687294368ULL},
	{0x1.31926e4851212p-2, -0x1.96badb7628e73p-2, 0x1.bc588233e7bf2p-1, 9509882626704415ULL},
	{0x1.f558b452ee202p-2, -0x1.0ddc237128897p-1, -0x1.63a4d34687cf8p-1, 17978707300750356ULL},
	{-0x1.5d8f3fdb5d757p-3, -0x1.af220b6393713p-1, 0x1.05f9d26c15264p-1, 10271384115672067ULL},
	{-0x1.7394a7f6b828dp-1, -0x1.9ba3f97bc860ap-5, -0x1.5f4c24914b2dap-1, 16748369454813597ULL},
	{-0x1.6867d8f1fc11bp-1, -0x1.8283144339067p-2, -0x1.3410921d4416cp-1, 16796017283055930ULL},
	{0x1.56dab84ba8806p-1, 0x1.d16c473d61aap-3, -0x1.6a0452121bcbp-1, 14559939633061979ULL},
	{-0x1.152ab4432c39ap-2, -0x1.17f9363a17d3fp-1, 0x1.95a6e4d920781p-1, 11148519967779278ULL},
	{0x1.371822d51281p-1, -0x1.88d4774682097p-3, -0x1.8a9cffa95c168p-1, 17742776871615534ULL},
	{-0x1.a34c5d68a328fp-2, 0x1.c1de1d09d3c3dp-1, -0x1.f6f4f350e3165p-3, 14821929197992010ULL},
	{0x1.9ea3f2cecf72ep-1, 0x1.d062ca58fa279p-2, 0x1.7d1224e0a7374p-2, 13335182634515454ULL},
	{0x1.358478403ca51p-1, -0x1.76fd64c27d0a3p-1, -0x1.40c8adc697014p-2, 17832137599461117ULL},
	{-0x1.37c26cdd70e44p-1, 0x1.94a0ad8ff97a5p-1, -0x1.182bc73380788p-4, 14794317925095234ULL},
	{-0x1.675de0e15208dp-3, -0x1.ebfd1d6605d46p-1, -0x1.b688f9df84f87p-3, 16369625195309063ULL},
	{0x1.cba8e4d08c0eap-2, 0x1.95025e9c46919p-1, 0x1.a9a0b065b8f19p-2, 13441088846710658ULL},
	{0x1.505758a30e102p-1, 0x1.2ca26b1ef62fbp-1, 0x1.e44dc2207cb0ep-2, 13484085317047451ULL},
	{-0x1.e802934e7b74dp-1, -0x1.427613bd1d4a9p-3, -0x1.08827f79e5a8fp-2, 16021798333789126ULL},
	{-0x1.4b9499f2a4e6ap-2, 0x1.253c3fc0c3d6bp-2, 0x1.cdb186e0e3ccdp-1, 11815454620263089ULL},
	{-0x1.45ee70fe05014p-2, 0x1.f27b758641748p-3, -0x1.d519e546faa27p-1, 15189429684718213ULL},
	{-0x1.62d69f7635957p-2, 0x1.6ac189616040bp-1, 0x1.3ac2d3d4c5c8p-1, 12126710747429794ULL},
	{0x1.a5f932f05d71dp-1, 0x1.11f5133bed616p-1, -0x1.7c19e577e2a49p-3, 13712422007855132ULL},
	{0x1.95939d2c4e637p-1, 0x1.8a61d6b572ef7p-2, 0x1.e4d6dae50cee8p-2, 13251094455783531ULL},
	{-0x1.18bea331f220cp-3, 0x1.3424020539c3ep-3, 0x1.f54826cc69ddcp-1, 11607840127842757ULL},
	{0x1.1d583cbfc492ap-3, -0x1.b86141259cceep-1, -0x1.f67a0551cd838p-2, 17027700790701224ULL},
	{-0x1.3a62264e9824fp-2, 0x1.8abe52a022b79p-1, 0x1.1dadc6343c994p-1, 12133156853694319ULL},
	{0x1.c79f710271f1ap-2, -0x1.74d870eecdd27p-3, -0x1.c0f3a0b29e631p-1, 17293478914903922ULL},
	{-0x1.9442743ba68b7p-1, 0x1.0b185a5e448e5p-1, 0x1.4aeda2c9f994cp-2, 12206598042915559ULL},
	{-0x1.a78d0f909b34bp-1, 0x1.fa64bb2c9952dp-3, -0x1.024e09966b064p-1, 15381600635842816ULL},
	{-0x1.48c01f9cd277bp-2, 0x1.4ea7236696a52p-1, -0x1.5ee6eac1de533p-1, 15689363599846822ULL},
	{0x1.75afcc4acea8dp-1, 0x1.38e1e0c5fc4fap-1, 0x1.39bb4ce707427p-2, 13359172889021358ULL},
	{-0x1.4dd4cb1fa99c8p-1, 0x1.09e6636caa1f7p-1, 0x1.1ad6cbed7dfb4p-1, 12320187638108450ULL},
	{0x1.8d570713880ffp-2, 0x1.153046553f47bp-1, -0x1.7de4188cd83e9p-1, 14405791649118276ULL},
	{0x1.900d7e80aaffep-2, 0x1.dfe8aadf62724p-2, -0x1.95a77995fd12ep-1, 14394055143467819ULL},
	{-0x1.b7cfa149c0737p-1, 0x1.ef8b00a5b52fap-2, -0x1.56378659db1a3p-3, 15325585725751670ULL},
	{0x1.2786fc14dbccap-2, 0x1.b92194fe99a4dp-1, -0x1.ab9ce40c2d21cp-2, 14259773996487997ULL},
	{-0x1.033ebe47fe2b7p-3, 0x1.14e61f50d407cp-1, 0x1.a9c23b9fce6a1p-1, 11677866650065162ULL},
};

// Whether the plain form returns the ids of IEEE-754 arithmetic on every probe.
bool plain_descent_is_exact() noexcept {
#ifdef HTM_FORCE_ROUNDED
	return false;
#else
	for (const auto& p : probes) {
		if (descend<false>(Cartesian(p.x, p.y, p.z)) != p.id) {
			return false;
		}
	}
	return true;
#endif
}

#endif  // HTM_FUSABLE

}  // namespace


uint64_t
HTM::getId(const Cartesian& coord)
{
#if HTM_FUSABLE
	static const bool plain = plain_descent_is_exact();
	return plain ? descend<false>(coord) : descend<true>(coord);
#else
	return descend<false>(coord);
#endif
}

#if defined(_MSC_VER)
#pragma fp_contract(on)
#endif
