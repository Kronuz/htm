// Trixel geometry helpers, names, ids, ranges and corners.
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

#include <algorithm>                              // for std::clamp
#include <cmath>                                  // for std::ceil, std::log2, std::pow, std::acos, std::sqrt
#include <format>                                 // for std::format
#include <stdexcept>                              // for std::invalid_argument
#include <tuple>                                  // for std::make_tuple
#include <utility>                                // for std::move


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
	// The dot of nearly equal unit vectors can round above 1; acos would give NaN.
	return std::acos(std::clamp(c1.center * c2.center, -1.0, 1.0)) < (c1.arcangle + c2.arcangle);
}


bool
HTM::insideVertex_Trixel( const Cartesian& v, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2)
{
	return (v0 ^ v1) * v >= 0 && (v1 ^ v2) * v >= 0 && (v2 ^ v0) * v >= 0;
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
