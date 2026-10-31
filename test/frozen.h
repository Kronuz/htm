// Reference HTM::getId / getTrixelName that build a Cartesian per midpoint: the
// oracle for the differential test and the benchmark. Keep them unchanged.
#pragma once

#include <cmath>
#include <cstdint>
#include <string>

#include "htm.h"

inline Cartesian frozen_mid(const Cartesian& v0, const Cartesian& v1) {
	auto w = v0 + v1;
	w.normalize();
	return w;
}

inline bool frozen_inside(const Cartesian& v, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) {
	return (v0 ^ v1) * v > 0 && (v1 ^ v2) * v > 0 && (v2 ^ v0) * v > 0;
}

inline uint64_t frozen_getId(const Cartesian& coord) {
	const auto& start_trixel = HTM::startTrixel(coord);
	auto v0 = start_vertices[start_trixel.v0];
	auto v1 = start_vertices[start_trixel.v1];
	auto v2 = start_vertices[start_trixel.v2];
	auto id = start_trixel.id;
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = frozen_mid(v0, v1);
		auto w0 = frozen_mid(v1, v2);
		auto w1 = frozen_mid(v2, v0);
		id <<= 2;
		if (frozen_inside(coord, v0, w2, w1)) {
			v1 = w2; v2 = w1;
		} else if (frozen_inside(coord, v1, w0, w2)) {
			id += 1; v0 = v1; v1 = w0; v2 = w2;
		} else if (frozen_inside(coord, v2, w1, w0)) {
			id += 2; v0 = v2; v1 = w1; v2 = w0;
		} else {
			id += 3; v0 = w0; v1 = w1; v2 = w2;
		}
	}
	return id;
}

inline std::string frozen_name_from_id(uint64_t id) {
	uint8_t last_pos = std::ceil(std::log2(id));
	++last_pos;
	last_pos &= ~1;
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

inline std::string frozen_getTrixelName(const Cartesian& coord) {
	const auto& start_trixel = HTM::startTrixel(coord);
	auto v0 = start_vertices[start_trixel.v0];
	auto v1 = start_vertices[start_trixel.v1];
	auto v2 = start_vertices[start_trixel.v2];
	auto name = start_trixel.name;
	int8_t depth = HTM_MAX_LEVEL;
	while (depth-- > 0) {
		auto w2 = frozen_mid(v0, v1);
		auto w0 = frozen_mid(v1, v2);
		auto w1 = frozen_mid(v2, v0);
		if (frozen_inside(coord, v0, w2, w1)) {
			name.push_back('0'); v1 = w2; v2 = w1;
		} else if (frozen_inside(coord, v1, w0, w2)) {
			name.push_back('1'); v0 = v1; v1 = w0; v2 = w2;
		} else if (frozen_inside(coord, v2, w1, w0)) {
			name.push_back('2'); v0 = v2; v1 = w1; v2 = w0;
		} else {
			name.push_back('3'); v0 = w0; v1 = w1; v2 = w2;
		}
	}
	return name;
}

