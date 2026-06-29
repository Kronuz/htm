/*
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
#include <cmath>                                  // for std::ceil, std::log2, std::pow, std::acos, std::sqrt, std::cos
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


std::string
HTM::getTrixelName(const Cartesian& coord)
{
	const auto& start_trixel = startTrixel(coord);
	auto v0 = start_vertices[start_trixel.v0];
	auto v1 = start_vertices[start_trixel.v1];
	auto v2 = start_vertices[start_trixel.v2];
	auto name = start_trixel.name;

	// Search in children's trixel
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = midPoint(v0, v1);
		auto w0 = midPoint(v1, v2);
		auto w1 = midPoint(v2, v0);
		if (insideVertex_Trixel(coord, v0, w2, w1)) {
			name.push_back('0');
			v1 = w2;
			v2 = w1;
		} else if (insideVertex_Trixel(coord, v1, w0, w2)) {
			name.push_back('1');
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if (insideVertex_Trixel(coord, v2, w1, w0)) {
			name.push_back('2');
			v0 = v2;
			v1 = w1;
			v2 = w0;
		} else {
			name.push_back('3');
			v0 = w0;
			v1 = w1;
			v2 = w2;
		}
	}

	return name;
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
HTM::getId(const Cartesian& coord)
{
	const auto& start_trixel = startTrixel(coord);
	auto v0 = start_vertices[start_trixel.v0];
	auto v1 = start_vertices[start_trixel.v1];
	auto v2 = start_vertices[start_trixel.v2];
	auto id = start_trixel.id;

	// Search in children's trixel
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = midPoint(v0, v1);
		auto w0 = midPoint(v1, v2);
		auto w1 = midPoint(v2, v0);
		id <<= 2;
		if (insideVertex_Trixel(coord, v0, w2, w1)) {
			v1 = w2;
			v2 = w1;
		} else if (insideVertex_Trixel(coord, v1, w0, w2)) {
			id += 1;
			v0 = v1;
			v1 = w0;
			v2 = w2;
		} else if (insideVertex_Trixel(coord, v2, w1, w0)) {
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
