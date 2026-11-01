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

#include <algorithm>                              // for std::merge, std::back_inserter
#include <cmath>                                  // for std::ceil, std::log2, std::pow, std::acos, std::sqrt, std::fmax, std::fabs
#include <cstring>                                // for std::memcpy
#include <format>                                 // for std::format
#include <iterator>                               // for std::make_move_iterator
#include <stdexcept>                              // for std::invalid_argument
#include <utility>                                // for std::move


std::vector<std::string>
HTM::trixel_union(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty()) {
		return std::move(txs2);
	}

	if (txs2.empty()) {
		return std::move(txs1);
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() + txs2.size());

	std::merge(std::make_move_iterator(txs1.begin()), std::make_move_iterator(txs1.end()),
		std::make_move_iterator(txs2.begin()), std::make_move_iterator(txs2.end()), std::back_inserter(res));

	return res;
}


std::vector<std::string>
HTM::trixel_intersection(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty() || txs2.empty()) {
		return std::vector<std::string>();
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() < txs2.size() ? txs1.size() : txs2.size());

	const auto it1_e = txs1.end();
	const auto it2_e = txs2.end();
	auto it1 = txs1.begin();
	auto it2 = txs2.begin();

	while (it1 != it1_e && it2 != it2_e) {
		if (*it1 > *it2) {
			if (it1->find(*it2) == 0) {
				res.push_back(std::move(*it1));
				++it1;
			} else {
				++it2;
			}
		} else {
			if (it2->find(*it1) == 0) {
				res.push_back(std::move(*it2));
				++it2;
			} else {
				++it1;
			}
		}
	}

	return res;
}


/*
 * Fills result with the trixels that conform to the father except trixel's son.
 *   Father      Son			 Trixels back:
 *     /\	     /\
 *    /__\      /__\	   =>	     __
 *   /\  /\					       /\  /\
 *  /__\/__\					  /__\/__\
 *
 * result is sort.
 */
static inline void exclusive_disjunction(std::vector<std::string>& result, const std::string& father, const std::string& son, size_t depth) {
	if (depth != 0u) {
		switch (son.at(father.length())) {
			case '0':
				exclusive_disjunction(result, father + std::string(1, '0'), son, --depth);
				result.push_back(father + std::string(1, '1'));
				result.push_back(father + std::string(1, '2'));
				result.push_back(father + std::string(1, '3'));
				return;
			case '1':
				result.push_back(father + std::string(1, '0'));
				exclusive_disjunction(result, father + std::string(1, '1'), son, --depth);
				result.push_back(father + std::string(1, '2'));
				result.push_back(father + std::string(1, '3'));
				return;
			case '2':
				result.push_back(father + std::string(1, '0'));
				result.push_back(father + std::string(1, '1'));
				exclusive_disjunction(result, father + std::string(1, '2'), son, --depth);
				result.push_back(father + std::string(1, '3'));
				return;
			case '3':
				result.push_back(father + std::string(1, '0'));
				result.push_back(father + std::string(1, '1'));
				result.push_back(father + std::string(1, '2'));
				exclusive_disjunction(result, father + std::string(1, '3'), son, --depth);
				return;
		}
	}
}


std::vector<std::string>
HTM::trixel_exclusive_disjunction(std::vector<std::string>&& txs1, std::vector<std::string>&& txs2)
{
	if (txs1.empty()) {
		return std::move(txs2);
	}

	if (txs2.empty()) {
		return std::move(txs1);
	}

	auto it1 = txs1.begin();
	auto it2 = txs2.begin();

	while (it1 != txs1.end() && it2 != txs2.end()) {
		if (*it1 > *it2) {
			if (it1->find(*it2) == 0) {
				size_t depth = it1->length() - it2->length();
				std::vector<std::string> subtrixels;
				subtrixels.reserve(3 * depth);
				exclusive_disjunction(subtrixels, *it2, *it1, depth);
				it2 = txs2.erase(it2);
				it2 = txs2.insert(it2, std::make_move_iterator(subtrixels.begin()), std::make_move_iterator(subtrixels.end()));
				it1 = txs1.erase(it1);
			} else {
				++it2;
			}
		} else {
			if (it2->find(*it1) == 0) {
				size_t depth = it2->length() - it1->length();
				std::vector<std::string> subtrixels;
				subtrixels.reserve(3 * depth);
				exclusive_disjunction(subtrixels, *it1, *it2, depth);
				it1 = txs1.erase(it1);
				it1 = txs1.insert(it1, std::make_move_iterator(subtrixels.begin()), std::make_move_iterator(subtrixels.end()));
				it2 = txs2.erase(it2);
			} else {
				++it1;
			}
		}
	}

	std::vector<std::string> res;
	res.reserve(txs1.size() + txs2.size());

	std::merge(std::make_move_iterator(txs1.begin()), std::make_move_iterator(txs1.end()),
		std::make_move_iterator(txs2.begin()), std::make_move_iterator(txs2.end()), std::back_inserter(res));

	return res;
}


std::vector<range_t>
HTM::range_union(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty()) {
		return std::move(rs2);
	}

	if (rs2.empty()) {
		return std::move(rs1);
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() + rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	res.push_back(it1->start < it2->start ? std::move(*it1++) : std::move(*it2++));

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			insertGreaterRange(res, std::move(*it1));
			++it1;
		} else {
			insertGreaterRange(res, std::move(*it2));
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start - 1) {   // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		}
		if (prev.end < it1->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it1->end;
		}
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start - 1) {  // (start - 1) for join adjacent integer ranges.
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		}
		if (prev.end < it2->end) {  // if ranges overlap, previous range end is updated.
			prev.end = it2->end;
		}
		++it2;
	}

	return res;
}


std::vector<range_t>
HTM::range_intersection(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty() || rs2.empty() || rs1.front().start > rs2.back().end || rs1.back().end < rs2.front().start) {
		return std::vector<range_t>();
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() < rs2.size() ? rs1.size() : rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			if (it1->end >= it2->start) {
				if (it1->end <= it2->end) {
					insertGreaterRange(res, range_t(it2->start, it1->end));
					++it1;
				} else {
					insertGreaterRange(res, *it2);
					++it2;
				}
			} else {
				++it1;
			}
		} else {
			if (it2->end >= it1->start) {
				if (it2->end <= it1->end) {
					insertGreaterRange(res, range_t(it1->start, it2->end));
					++it2;
				} else {
					insertGreaterRange(res, *it1);
					++it1;
				}
			} else {
				++it2;
			}
		}
	}

	return res;
}


static inline void exclusive_disjunction(std::vector<range_t>& res, range_t& range) {
	if (res.empty()) {
		res.push_back(std::move(range));
		return;
	}

	auto& prev = res.back();
	if (prev.start == range.start) {
		if (prev.end > range.end) {
			prev.start = range.end + 1;
		} else if (prev.end < range.end) {
			prev.start = prev.end + 1;
			prev.end = range.end;
		} else {
			res.pop_back();
		}
	} else {
		if (range.start < prev.start) {
			std::swap(prev, range);
		}
		if (prev.end < range.start) {
			HTM::insertGreaterRange(res, std::move(range));
		} else {
			if (prev.end == range.end) {
				prev.end = --range.start;
			} else if (prev.end < range.end) {
				std::swap(++prev.end, --range.start);
				HTM::insertGreaterRange(res, std::move(range));
			} else {
				std::swap(prev.end, --range.start);
				std::swap(range.start, ++range.end);
				HTM::insertGreaterRange(res, std::move(range));
			}
		}
	}
}


std::vector<range_t>
HTM::range_exclusive_disjunction(std::vector<range_t>&& rs1, std::vector<range_t>&& rs2)
{
	if (rs1.empty()) {
		return std::move(rs2);
	}

	if (rs2.empty()) {
		return std::move(rs1);
	}

	std::vector<range_t> res;
	res.reserve(rs1.size() + rs2.size());

	const auto it1_e = rs1.end();
	const auto it2_e = rs2.end();
	auto it1 = rs1.begin();
	auto it2 = rs2.begin();

	res.push_back(it1->start < it2->start ? *it1++ : *it2++);

	while (it1 != it1_e && it2 != it2_e) {
		if (it1->start < it2->start) {
			exclusive_disjunction(res, *it1);
			++it1;
		} else {
			exclusive_disjunction(res, *it2);
			++it2;
		}
	}

	while (it1 != it1_e) {
		auto& prev = res.back();
		if (prev.end < it1->start) {
			res.insert(res.end(), std::make_move_iterator(it1), std::make_move_iterator(it1_e));
			break;
		}
		exclusive_disjunction(res, *it1);
		++it1;
	}

	while (it2 != it2_e) {
		auto& prev = res.back();
		if (prev.end < it2->start) {
			res.insert(res.end(), std::make_move_iterator(it2), std::make_move_iterator(it2_e));
			break;
		}
		exclusive_disjunction(res, *it2);
		++it2;
	}

	return res;
}


const trixel_t&
HTM::startTrixel(const Cartesian& coord) noexcept
{
	int num;
	if (coord.x > 0 && coord.y >= 0) {
		num = (coord.z >= 0) ? 3 : 4; // N3  S0
	} else if (coord.x <= 0 && coord.y > 0) {
		num = (coord.z >= 0) ? 2 : 5; // N2  S1
	} else if (coord.x < 0 && coord.y <= 0) {
		num = (coord.z >= 0) ? 1 : 6; // N1  S2
	} else if (coord.x >= 0 && coord.y < 0) {
		num = (coord.z >= 0) ? 0 : 7; // N0  S3
	} else {
		num = (coord.z >= 0) ? 3 : 4; // N3  S0
	}
	return start_trixels[num];
}


Cartesian
HTM::midPoint(const Cartesian& v0, const Cartesian& v1)
{
	auto w = v0 + v1;
	w.normalize();
	return w;
}


bool
HTM::thereisHole(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	return (v0 ^ v1) * c.center < 0.0 && (v1 ^ v2) * c.center < 0.0 && (v2 ^ v0) * c.center < 0.0;
}


Constraint
HTM::getBoundingCircle(const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	// Normal vector to the triangle plane.
	Constraint boundingCircle((v1 - v0) ^ (v2 - v1));
	boundingCircle.arcangle = std::acos(v0 * boundingCircle.center);
	return boundingCircle;
}


bool
HTM::intersectConstraints(const Constraint& c1, const Constraint& c2)
{
	return std::acos(c1.center * c2.center) < (c1.arcangle + c2.arcangle);
}


bool
HTM::insideVertex_Trixel( const Cartesian& v, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	return (v0 ^ v1) * v > 0 && (v1 ^ v2) * v > 0 && (v2 ^ v0) * v > 0;
}


bool
HTM::insideVertex_Constraint(const Cartesian& v, const Constraint& c)
{
	return c.center * v > c.distance;
}


bool
HTM::intersectConstraint_EdgeTrixel(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	return (
		intersection(c, v0, v1) ||
		intersection(c, v1, v2) ||
		intersection(c, v2, v0)
	);
}


bool
HTM::intersection(const Constraint& c, const Cartesian& v1, const Cartesian& v2)
{
	double gamma1 = v1 * c.center;
	double gamma2 = v2 * c.center;
	double cos_t = v1 * v2;
	double square_u = (1 - cos_t) / (1 + cos_t);

	double _a = - square_u * (gamma1 + c.distance);
	double _b = gamma1 * (square_u - 1) + gamma2 * (square_u + 1);
	double _c = gamma1 - c.distance;
	double aux = (_b * _b) - (4 * _a * _c);

	if (aux < 0.0 || (_a > -DBL_TOLERANCE && _a < DBL_TOLERANCE)) {
		return false;
	}

	aux = std::sqrt(aux);
	_a = 2 * _a;
	_b = -_b;
	double r1 = (_b + aux) / _a;
	double r2 = (_b - aux) / _a;

	return ((r1 >= 0.0 && r1 <= 1.0) || (r2 >= 0.0 && r2 <= 1.0));
}


void
HTM::simplifyTrixels(std::vector<std::string>& trixels)
{
	if (trixels.empty()) {
		return;
	}

	// Delete duplicates and redundants.
	for (auto it = trixels.begin(); it != trixels.end() - 1; ) {
		auto n_it = it + 1;
		if (n_it->find(*it) == 0) {
			trixels.erase(n_it);
		} else {
			++it;
		}
	}

	// To compact.
	for (auto it = trixels.begin(); trixels.size() > 3 && it < trixels.end() - 3; ) {
		auto tlen = it->length();
		if (tlen > 2) {
			auto it_j = it + 1;
			auto it_k = it + 2;
			auto it_l = it + 3;
			if (it_j->length() == tlen && it_k->length() == tlen && it_l->length() == tlen) {
				--tlen;
				bool equals = true;
				for (size_t p = 0; p < tlen; ++p) {
					const auto& c = (*it)[p];
					if ((*it_j)[p] != c || (*it_k)[p] != c || (*it_l)[p] != c) {
						equals = false;
						break;
					}
				}
				if (equals) {
					it_j = trixels.erase(it_j);
					it_j = trixels.erase(it_j);
					trixels.erase(it_j);
					it->pop_back();
					it_j = trixels.begin();
					it < it_j + 3 ? it = it_j : it -= 3;
					continue;
				}
			}
		}
		++it;
	}
}


void
HTM::simplifyRanges(std::vector<range_t>& ranges)
{
	if (!ranges.empty()) {
		for (auto it = ranges.begin() + 1; it != ranges.end(); ) {
			// Get previous range.
			auto tmp = it - 1;
			// Test if current range is not overlapping with previous.
			if (tmp->end < it->start - 1) {   // (start - 1) for join adjacent integer ranges.
				++it;
			} else if (tmp->end < it->end) {  // if ranges overlap, previous range end is updated.
				tmp->end = it->end;
				it = ranges.erase(it);
			} else {
				// Ranges overlapping.
				it = ranges.erase(it);
			}
		}
	}
}


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


// (a ^ b) * c > 0, as in HTM::insideVertex_Trixel's building block.
template <bool Round>
HTM_NOFUSE_FN inline double triple(const V3& a, const V3& b, const V3& c) noexcept {
	HTM_NOFUSE_BODY
	double cx = HTM_MUL(Round, a.y, b.z) - HTM_MUL(Round, b.y, a.z);
	double cy = HTM_MUL(Round, a.z, b.x) - HTM_MUL(Round, b.z, a.x);
	double cz = HTM_MUL(Round, a.x, b.y) - HTM_MUL(Round, b.x, a.y);
	return HTM_MUL(Round, cx, c.x) + HTM_MUL(Round, cy, c.y) + HTM_MUL(Round, cz, c.z);
}


template <bool Round>
HTM_NOFUSE_FN inline bool inside(const V3& v, const V3& v0, const V3& v1, const V3& v2) noexcept {
	HTM_NOFUSE_BODY
	return triple<Round>(v0, v1, v) > 0 && triple<Round>(v1, v2, v) > 0 && triple<Round>(v2, v0, v) > 0;
}


// The walk the ids are defined by: at each level the three midpoints, then the
// first child whose three edge tests are positive, else the middle child. getId
// takes it for an input outside the range the descent below is proved for.
template <bool Round>
HTM_NOFUSE_FN uint64_t walk(const V3& c, const trixel_t& start_trixel) noexcept {
	HTM_NOFUSE_BODY
	const auto& s0 = start_vertices[start_trixel.v0];
	const auto& s1 = start_vertices[start_trixel.v1];
	const auto& s2 = start_vertices[start_trixel.v2];
	V3 v0{s0.x, s0.y, s0.z};
	V3 v1{s1.x, s1.y, s1.z};
	V3 v2{s2.x, s2.y, s2.z};
	auto id = start_trixel.id;

	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = midpoint<Round>(v0, v1);
		auto w0 = midpoint<Round>(v1, v2);
		auto w1 = midpoint<Round>(v2, v0);
		id <<= 2;
		if (inside<Round>(c, v0, w2, w1)) {
			v1 = w2;
			v2 = w1;
		} else if (inside<Round>(c, v1, w0, w2)) {
			id += 1;
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if (inside<Round>(c, v2, w1, w0)) {
			id += 2;
			v0 = v2;
			v1 = w1;
			v2 = w0;
		} else {
			id += 3;
			v0 = w0;
			v1 = w1;
			v2 = w2;
		}
	}

	return id;
}


// RN(sqrt(q)) for q within 2^26 units in the last place of 4: halving the bits of
// q (exponent and fraction together) is then exact (test/sqrt_test.cc checks every
// such q). The sums of squares of the midpoints from depth HTM_NEAR4_DEPTH on are
// within 2^19 units of 4.
inline double sqrt_near4(double q) noexcept {
	uint64_t b;
	std::memcpy(&b, &q, sizeof b);
	b = (b >> 1) + 0x1FF8000000000000ULL;
	std::memcpy(&q, &b, sizeof q);
	return q;
}


template <bool Round, bool Near4>
HTM_NOFUSE_FN inline V3 midpoint_at(const V3& a, const V3& b) noexcept {
	HTM_NOFUSE_BODY
	if constexpr (Near4) {
		V3 w{a.x + b.x, a.y + b.y, a.z + b.z};
		double n = sqrt_near4(HTM_MUL(Round, w.x, w.x) + HTM_MUL(Round, w.y, w.y) + HTM_MUL(Round, w.z, w.z));
		return {w.x / n, w.y / n, w.z / n};
	} else {
		return midpoint<Round>(a, b);
	}
}


// A trixel of the walk and the tests of its edges against the point:
// e0 = (v1 ^ v2) * c, e1 = (v2 ^ v0) * c, e2 = (v0 ^ v1) * c, as triple() rounds them.
struct Trixel {
	V3 v0, v1, v2;
	double e0, e1, e2;
};


inline const V3& pick(int d, const V3& a, const V3& b, const V3& c, const V3& e) noexcept {
	return d == 0 ? a : d == 1 ? b : d == 2 ? c : e;
}


// One level of the walk, without its branches. The child is predicted from the
// parent's edge tests: the inner edge w1-w2 tests as e1 + e2 - e0 (over the two
// midpoint norms, a positive factor), w2-w0 as e0 + e2 - e1 and w0-w1 as
// e0 + e1 - e2, so the point is in child 0 when e1 + e2 - e0 < 0, and so on. The
// child's own three edge tests, computed as the walk computes them, are then the
// walk's three tests for that child, and are kept for the next level. They decide
// the child alone for children 0 and 3. Children 1 and 2 also need the walk's
// earlier children to fail: child 1 needs (w2 ^ w1) * c <= 0, child 2 that and
// (w0 ^ w2) * c <= 0. Above HTM_EXACT_DEPTH a margin of 2^-44 max|c| on
// e1 + e2 - e0 (e0 + e2 - e1) proves it, as below; past it, or without the
// margin, the test is computed. Whatever fails the checks takes the walk's own
// branches at this level.
//
// The margin. Each fp vertex v of the mesh is within 5.5u (u = 2^-53) of the unit
// vector along the exact sum of its parents, and a computed triple product is
// within 11.3u max|c| of the exact one. With m1 = |v2 + v0|, m2 = |v0 + v1| (both
// at most 2 + 7u), (w1 ^ w2) * c is (e1 + e2 - e0) / (m1 m2) to within 30.4u
// max|c| plus the three tests' error over m1 m2, and the sum rounds within 10.4u
// max|c|, so (w2 ^ w1) * c < 0 when the rounded e1 + e2 - e0 exceeds 166u max|c|.
// 2^-44 is 512u.
template <bool Round, bool Near4, bool Certify>
HTM_NOFUSE_FN inline void step(Trixel& t, uint64_t& id, const V3& c, double margin) noexcept {
	HTM_NOFUSE_BODY
	const V3 w0 = midpoint_at<Round, Near4>(t.v1, t.v2);
	const V3 w1 = midpoint_at<Round, Near4>(t.v2, t.v0);
	const V3 w2 = midpoint_at<Round, Near4>(t.v0, t.v1);
	const double fa = (t.e1 + t.e2) - t.e0;
	const double fb = (t.e0 + t.e2) - t.e1;
	const double fc = (t.e0 + t.e1) - t.e2;
	int d = fa < 0 ? 0 : fb < 0 ? 1 : fc < 0 ? 2 : 3;
	Trixel n{pick(d, t.v0, t.v1, t.v2, w0), pick(d, w2, w0, w1, w1), pick(d, w1, w2, w0, w2), 0, 0, 0};
	n.e0 = triple<Round>(n.v1, n.v2, c);
	n.e1 = triple<Round>(n.v2, n.v0, c);
	n.e2 = triple<Round>(n.v0, n.v1, c);
	bool ok = n.e0 > 0 && n.e1 > 0 && n.e2 > 0;
	if constexpr (Certify) {
		const bool need_a = (d == 1 || d == 2) && !(fa > margin);
		const bool need_b = d == 2 && !(fb > margin);
		if (need_a || need_b) [[unlikely]] {
			if (need_a) {
				ok = ok && triple<Round>(w1, w2, c) > 0;
			}
			if (need_b) {
				ok = ok && triple<Round>(w2, w0, c) > 0;
			}
		}
	} else {
		const double ia = triple<Round>(w1, w2, c);
		const double ib = triple<Round>(w2, w0, c);
		ok = ok && ((d != 1 && d != 2) || ia > 0) && (d != 2 || ib > 0);
	}
	if (!ok) [[unlikely]] {
		if (inside<Round>(c, t.v0, w2, w1)) {
			d = 0;
		} else if (inside<Round>(c, t.v1, w0, w2)) {
			d = 1;
		} else if (inside<Round>(c, t.v2, w1, w0)) {
			d = 2;
		} else {
			d = 3;
		}
		n.v0 = pick(d, t.v0, t.v1, t.v2, w0);
		n.v1 = pick(d, w2, w0, w1, w1);
		n.v2 = pick(d, w1, w2, w0, w2);
		n.e0 = triple<Round>(n.v1, n.v2, c);
		n.e1 = triple<Round>(n.v2, n.v0, c);
		n.e2 = triple<Round>(n.v0, n.v1, c);
	}
	id = (id << 2) | static_cast<uint64_t>(d);
	t = n;
}


// Depths [0, HTM_NEAR4_DEPTH) take std::sqrt, the rest sqrt_near4; depths
// [HTM_EXACT_DEPTH, HTM_MAX_LEVEL) compute every test they need.
constexpr int HTM_NEAR4_DEPTH = 14;
constexpr int HTM_EXACT_DEPTH = 21;


template <bool Round>
HTM_NOFUSE_FN uint64_t descend(const Cartesian& coord) noexcept {
	HTM_NOFUSE_BODY
	const V3 c{coord.x, coord.y, coord.z};
	const auto& start_trixel = HTM::startTrixel(coord);
	const double scale = std::fmax(std::fabs(c.x), std::fmax(std::fabs(c.y), std::fabs(c.z)));
	if (!(scale >= 0x1p-500 && scale <= 0x1p500)) {
		return walk<Round>(c, start_trixel);
	}
	const double margin = scale * 0x1p-44;
	const auto& s0 = start_vertices[start_trixel.v0];
	const auto& s1 = start_vertices[start_trixel.v1];
	const auto& s2 = start_vertices[start_trixel.v2];
	Trixel t{{s0.x, s0.y, s0.z}, {s1.x, s1.y, s1.z}, {s2.x, s2.y, s2.z}, 0, 0, 0};
	t.e0 = triple<Round>(t.v1, t.v2, c);
	t.e1 = triple<Round>(t.v2, t.v0, c);
	t.e2 = triple<Round>(t.v0, t.v1, c);
	uint64_t id = start_trixel.id;
	int depth = 0;
	for (; depth < HTM_NEAR4_DEPTH; ++depth) {
		step<Round, false, true>(t, id, c, margin);
	}
	for (; depth < HTM_EXACT_DEPTH; ++depth) {
		step<Round, true, true>(t, id, c, margin);
	}
	for (; depth < static_cast<int>(HTM_MAX_LEVEL); ++depth) {
		step<Round, true, false>(t, id, c, margin);
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
	{0x1.7143929ab78c7p-2, 0x1.9994a0e5273f7p-2, 0x1.af695eb632132p-1, 12881359809030655ULL},
	{-0x1.6cc5e93b0fc1ap-2, -0x1.a84422bf27e89p-2, 0x1.accf90d8ebafdp-1, 10594081174125255ULL},
	{-0x1.631dc7ed6cf1bp-1, 0x1.0a9c416314d73p-1, 0x1.fdba02043df9ap-2, 12328104190415457ULL},
	{-0x1.922d5eb73ca42p-3, 0x1.4d2eb767b8bb9p-3, -0x1.ef1288d89ea8ap-1, 14971743887750959ULL},
	{0x1.e24a85ff2af6cp-2, -0x1.4ddfb122ffc1fp-2, 0x1.a3ac201d72806p-1, 9466734108671887ULL},
	{0x1.90bc362868237p-4, 0x1.d1f5e6b05aa85p-1, 0x1.9c62d501f1376p-2, 12504775144506823ULL},
	{-0x1.27086062ad855p-2, -0x1.c73aec5dedd3dp-1, -0x1.6c2256778b33dp-2, 16542991213780927ULL},
	{-0x1.98f85900936ccp-1, -0x1.a774271765c69p-2, 0x1.bf7a6bfb9266fp-2, 11223956368872415ULL},
	{0x1.85e3d16c6a0bap-1, 0x1.2747b70964e6ep-1, -0x1.2eea71ee71587p-2, 14454870337873790ULL},
	{0x1.f252c0c4e9f8ep-2, -0x1.81d6f763ab2f8p-1, -0x1.c481497e203a9p-2, 17947105761604127ULL},
	{0x1.37a5abb8c22ap-1, 0x1.82d277f5243e7p-2, 0x1.653a7fd168122p-1, 13277068919624078ULL},
	{0x1.aa216a26fe8dfp-2, 0x1.7f9b83b947addp-2, -0x1.a837160fe4513p-1, 13968848208776055ULL},
	{-0x1.fda1b2c7fac9cp-3, -0x1.17c8c431e2bcep-1, 0x1.996dca9a3f4fep-1, 10595698687294368ULL},
	{0x1.31926e4851212p-2, -0x1.96badb7628e73p-2, 0x1.bc588233e7bf2p-1, 9509882626704415ULL},
	{0x1.f558b452ee202p-2, -0x1.0ddc237128897p-1, -0x1.63a4d34687cf8p-1, 17978707300750359ULL},
	{-0x1.5d8f3fdb5d757p-3, -0x1.af220b6393713p-1, 0x1.05f9d26c15264p-1, 10271384115672067ULL},
	{-0x1.7394a7f6b828dp-1, -0x1.9ba3f97bc860ap-5, -0x1.5f4c24914b2dap-1, 16748369454813599ULL},
	{-0x1.6867d8f1fc11bp-1, -0x1.8283144339067p-2, -0x1.3410921d4416cp-1, 16796017283055930ULL},
	{0x1.56dab84ba8806p-1, 0x1.d16c473d61aap-3, -0x1.6a0452121bcbp-1, 14559939633061979ULL},
	{-0x1.152ab4432c39ap-2, -0x1.17f9363a17d3fp-1, 0x1.95a6e4d920781p-1, 11148519967779279ULL},
	{0x1.371822d51281p-1, -0x1.88d4774682097p-3, -0x1.8a9cffa95c168p-1, 17742776871615534ULL},
	{-0x1.a34c5d68a328fp-2, 0x1.c1de1d09d3c3dp-1, -0x1.f6f4f350e3165p-3, 14821929197992010ULL},
	{0x1.9ea3f2cecf72ep-1, 0x1.d062ca58fa279p-2, 0x1.7d1224e0a7374p-2, 13335182634515455ULL},
	{0x1.358478403ca51p-1, -0x1.76fd64c27d0a3p-1, -0x1.40c8adc697014p-2, 17832137599461117ULL},
	{-0x1.37c26cdd70e44p-1, 0x1.94a0ad8ff97a5p-1, -0x1.182bc73380788p-4, 14794317925095235ULL},
	{-0x1.675de0e15208dp-3, -0x1.ebfd1d6605d46p-1, -0x1.b688f9df84f87p-3, 16369625195309063ULL},
	{0x1.cba8e4d08c0eap-2, 0x1.95025e9c46919p-1, 0x1.a9a0b065b8f19p-2, 13441088846710658ULL},
	{0x1.505758a30e102p-1, 0x1.2ca26b1ef62fbp-1, 0x1.e44dc2207cb0ep-2, 13484085317047451ULL},
	{-0x1.e802934e7b74dp-1, -0x1.427613bd1d4a9p-3, -0x1.08827f79e5a8fp-2, 16021798333789127ULL},
	{-0x1.4b9499f2a4e6ap-2, 0x1.253c3fc0c3d6bp-2, 0x1.cdb186e0e3ccdp-1, 11815454620263091ULL},
	{-0x1.45ee70fe05014p-2, 0x1.f27b758641748p-3, -0x1.d519e546faa27p-1, 15189429684718213ULL},
	{-0x1.62d69f7635957p-2, 0x1.6ac189616040bp-1, 0x1.3ac2d3d4c5c8p-1, 12126710747429795ULL},
	{0x1.a5f932f05d71dp-1, 0x1.11f5133bed616p-1, -0x1.7c19e577e2a49p-3, 13712422007855132ULL},
	{0x1.95939d2c4e637p-1, 0x1.8a61d6b572ef7p-2, 0x1.e4d6dae50cee8p-2, 13251094455783531ULL},
	{-0x1.18bea331f220cp-3, 0x1.3424020539c3ep-3, 0x1.f54826cc69ddcp-1, 11607840127842757ULL},
	{0x1.1d583cbfc492ap-3, -0x1.b86141259cceep-1, -0x1.f67a0551cd838p-2, 17027700790701227ULL},
	{-0x1.3a62264e9824fp-2, 0x1.8abe52a022b79p-1, 0x1.1dadc6343c994p-1, 12133156853694319ULL},
	{0x1.c79f710271f1ap-2, -0x1.74d870eecdd27p-3, -0x1.c0f3a0b29e631p-1, 17293478914903922ULL},
	{-0x1.9442743ba68b7p-1, 0x1.0b185a5e448e5p-1, 0x1.4aeda2c9f994cp-2, 12206598042915559ULL},
	{-0x1.a78d0f909b34bp-1, 0x1.fa64bb2c9952dp-3, -0x1.024e09966b064p-1, 15381600635842831ULL},
	{-0x1.48c01f9cd277bp-2, 0x1.4ea7236696a52p-1, -0x1.5ee6eac1de533p-1, 15689363599846823ULL},
	{0x1.75afcc4acea8dp-1, 0x1.38e1e0c5fc4fap-1, 0x1.39bb4ce707427p-2, 13359172889021359ULL},
	{-0x1.4dd4cb1fa99c8p-1, 0x1.09e6636caa1f7p-1, 0x1.1ad6cbed7dfb4p-1, 12320187638108450ULL},
	{0x1.8d570713880ffp-2, 0x1.153046553f47bp-1, -0x1.7de4188cd83e9p-1, 14405791649118335ULL},
	{0x1.900d7e80aaffep-2, 0x1.dfe8aadf62724p-2, -0x1.95a77995fd12ep-1, 14394055143467819ULL},
	{-0x1.b7cfa149c0737p-1, 0x1.ef8b00a5b52fap-2, -0x1.56378659db1a3p-3, 15325585725751670ULL},
	{0x1.2786fc14dbccap-2, 0x1.b92194fe99a4dp-1, -0x1.ab9ce40c2d21cp-2, 14259773996487999ULL},
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


std::string
HTM::getTrixelName(const Cartesian& coord)
{
	return getTrixelName(getId(coord));
}


std::string
HTM::getTrixelName(uint64_t id)
{
	uint8_t last_pos = std::ceil(std::log2(id));
	++last_pos;
	last_pos &= ~1;  // Must be multiple of two.
	std::string trixel;
	trixel.reserve(last_pos / 2);
	last_pos -= 2;
	uint64_t mask = static_cast<uint64_t>(3) << last_pos;
	trixel.push_back(((id & mask) >> last_pos) == 3 ? 'S' : 'N');
	while ((mask >>= 2) != 0u) {
		last_pos -= 2;
		trixel.push_back('0' + ((id & mask) >> last_pos));
	}

	return trixel;
}


uint64_t
HTM::getId(const std::string& name)
{
	const auto len = name.length();

	uint64_t id = name[0] == 'N' ? 2 : 3;

	for (size_t i = 1; i < len; ++i) {
		id <<= 2;
		id |= name[i] - '0';
	}

	return id;
}


range_t
HTM::getRange(uint64_t id, uint8_t level)
{
	if (level < HTM_MAX_LEVEL) {
		int8_t mask = (HTM_MAX_LEVEL - level) << 1;
		uint64_t start = id << mask;
		return {start, start + ((uint64_t) 1 << mask) - 1};
	}
	return {id, id};
}


range_t
HTM::getRange(const std::string& name)
{
	return getRange(getId(name), name.length() - 2);
}


static inline void get_trixels(std::vector<std::string>& trixels, uint64_t start, uint64_t end) {
	if (start == end) {
		trixels.push_back(HTM::getTrixelName(start));
		return;
	}

	uint8_t log_inc = std::ceil(std::log2(end - start));
	log_inc &= ~1;  // Must be multiple of two.
	uint64_t max_inc = std::pow(2, log_inc);

	uint64_t mod = start % max_inc;
	uint64_t _start = mod != 0u ? start + max_inc - mod : start;

	while (end < (_start + max_inc - 1) || log_inc > HTM_START_POS) {
		log_inc -= 2;
		max_inc = std::pow(2, log_inc);
		mod = start % max_inc;
		_start = mod != 0u ? start + max_inc - mod : start;
	}

	if (_start > start) {
		get_trixels(trixels, start, _start - 1);
	}

	uint64_t _end = end - max_inc + 2;
	while (_start < _end) {
		trixels.push_back(HTM::getTrixelName(_start >> log_inc));
		_start += max_inc;
	}

	if (_start <= end) {
		get_trixels(trixels, _start, end);
	}
}


std::vector<std::string>
HTM::getTrixels(const std::vector<range_t>& ranges)
{
	std::vector<std::string> trixels;
	trixels.reserve(ranges.size());
	for (const auto& range : ranges) {
		get_trixels(trixels, range.start, range.end);
	}

	return trixels;
}


static inline void get_id_trixels(std::vector<uint64_t>& id_trixels, uint64_t start, uint64_t end) {
	if (start == end) {
		id_trixels.push_back(start);
		return;
	}

	uint8_t log_inc = std::ceil(std::log2(end - start));
	log_inc &= ~1;  // Must be multiple of two.
	uint64_t max_inc = std::pow(2, log_inc);

	uint64_t mod = start % max_inc;
	uint64_t _start = mod != 0u ? start + max_inc - mod : start;

	while (end < (_start + max_inc - 1) || log_inc > HTM_START_POS) {
		log_inc -= 2;
		max_inc = std::pow(2, log_inc);
		mod = start % max_inc;
		_start = mod != 0u ? start + max_inc - mod : start;
	}

	if (_start > start) {
		get_id_trixels(id_trixels, start, _start - 1);
	}

	uint64_t _end = end - max_inc + 2;
	while (_start < _end) {
		id_trixels.push_back(_start >> log_inc);
		_start += max_inc;
	}

	if (_start <= end) {
		get_id_trixels(id_trixels, _start, end);
	}
}


std::vector<uint64_t>
HTM::getIdTrixels(const std::vector<range_t>& ranges)
{
	std::vector<uint64_t> id_trixels;
	id_trixels.reserve(ranges.size());
	for (const auto& range : ranges) {
		get_id_trixels(id_trixels, range.start, range.end);
	}

	return id_trixels;
}


std::tuple<Cartesian, Cartesian, Cartesian>
HTM::getCorners(const std::string& name)
{
	const auto& start_trixel = start_trixels[name[1] - '0' + (name[0] == 'S' ? 4 : 0)];
	Cartesian v0 = start_vertices[start_trixel.v0];
	Cartesian v1 = start_vertices[start_trixel.v1];
	Cartesian v2 = start_vertices[start_trixel.v2];

	size_t i = 2, len = name.length();
	while (i < len) {
		auto w2 = midPoint(v0, v1);
		auto w0 = midPoint(v1, v2);
		auto w1 = midPoint(v2, v0);
		switch (name[i]) {
			case '0':
				v1 = w2;
				v2 = w1;
				break;
			case '1':
				v0 = v1;
				v1 = w0;
				v2 = w2;
				break;
			case '2':
				v0 = v2;
				v1 = w1;
				v2 = w0;
				break;
			case '3':
				v0 = w0;
				v1 = w1;
				v2 = w2;
				break;
			default:
				throw std::invalid_argument(std::format("Invalid trixel's name: {}", name));
		}
		++i;
	}

	return std::make_tuple(std::move(v0), std::move(v1), std::move(v2));
}
