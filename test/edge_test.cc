// Points on the edges of the root mesh: the equator, the meridians 0, 90, 180 and
// 270, and the poles. Each gets the id of a trixel that holds it, and a 1 km circle
// around it covers that id (the circle cover of Xapiand's geospatial Circle, built
// from the same HTM helpers). With open triangles such a point fell through to
// child 3 at every level and got an id thousands of km away.
//
// Usage: htm_edge_test
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <tuple>
#include <vector>

#include "htm.h"

static size_t checked = 0, failures = 0;


static TypeTrixel verify(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2) {
	int sum = (HTM::insideVertex_Constraint(v0, c) ? 1 : 0) + (HTM::insideVertex_Constraint(v1, c) ? 1 : 0) +
		(HTM::insideVertex_Constraint(v2, c) ? 1 : 0);
	if (sum == 3) {
		return TypeTrixel::FULL;
	}
	if (sum > 0) {
		return TypeTrixel::PARTIAL;
	}
	if (HTM::intersectConstraints(c, HTM::getBoundingCircle(v0, v1, v2)) &&
		(HTM::intersectConstraint_EdgeTrixel(c, v0, v1, v2) || HTM::insideVertex_Trixel(c.center, v0, v1, v2))) {
		return TypeTrixel::PARTIAL;
	}
	return TypeTrixel::OUTSIDE;
}


static void lookup(const Constraint& c, const Cartesian& v0, const Cartesian& v1, const Cartesian& v2, uint64_t id,
	uint8_t level, uint8_t max_level, std::vector<range_t>& ranges) {
	if (level == max_level) {
		ranges.push_back(HTM::getRange(id, level));
		return;
	}
	auto w2 = HTM::midPoint(v0, v1), w0 = HTM::midPoint(v1, v2), w1 = HTM::midPoint(v2, v0);
	const Cartesian children[4][3] = {{v0, w2, w1}, {v1, w0, w2}, {v2, w1, w0}, {w0, w1, w2}};
	for (uint64_t i = 0; i < 4; ++i) {
		const auto& t = children[i];
		switch (verify(c, t[0], t[1], t[2])) {
			case TypeTrixel::FULL:
				ranges.push_back(HTM::getRange((id << 2) + i, level + 1));
				break;
			case TypeTrixel::PARTIAL:
				lookup(c, t[0], t[1], t[2], (id << 2) + i, level + 1, max_level, ranges);
				break;
			default:
				break;
		}
	}
}


// The ranges of full-depth ids a circle covers, at the level its error allows (error 0.3).
static std::vector<range_t> cover(const Constraint& c) {
	uint8_t max_level = HTM_MAX_LEVEL;
	for (uint8_t i = 0; i < HTM_MAX_LEVEL; ++i) {
		if (ERROR_NIVEL[i] < 0.3 * c.radius) {
			max_level = i;
			break;
		}
	}
	std::vector<range_t> ranges;
	for (const auto& t : start_trixels) {
		const auto& v0 = start_vertices[t.v0];
		const auto& v1 = start_vertices[t.v1];
		const auto& v2 = start_vertices[t.v2];
		if (verify(c, v0, v1, v2) != TypeTrixel::OUTSIDE) {
			lookup(c, v0, v1, v2, t.id, 0, max_level, ranges);
		}
	}
	return ranges;
}


static void check(const char* what, Cartesian p) {
	p.normalize();
	++checked;
	uint64_t id = HTM::getId(p);
	auto name = HTM::getTrixelName(id);

	// The trixel holds the point: on the inner side of each edge, within rounding.
	auto [v0, v1, v2] = HTM::getCorners(name);
	const double eps = -1e-15;
	bool holds = (v0 ^ v1) * p >= eps && (v1 ^ v2) * p >= eps && (v2 ^ v0) * p >= eps;

	// A 1 km circle around the point covers its id.
	bool covered = false;
	for (const auto& r : cover(Constraint(p, 1000.0))) {
		covered = covered || (r.start <= id && id <= r.end);
	}

	// The same point with signed zeros flipped gets the same id.
	Cartesian q(p.x == 0 ? -p.x : p.x, p.y == 0 ? -p.y : p.y, p.z == 0 ? -p.z : p.z);
	bool same = HTM::getId(q) == id;

	if (!holds || !covered || !same) {
		++failures;
		std::printf("FAIL %s (%.17g %.17g %.17g): %s%s%s%s\n", what, p.x, p.y, p.z, name.c_str(),
			holds ? "" : " does not hold it", covered ? "" : ", 1 km circle misses it", same ? "" : ", signed zeros differ");
	}
}


static void check_degrees(double lat, double lon) {
	char what[64];
	std::snprintf(what, sizeof what, "lat %g lon %g", lat, lon);
	check(what, Cartesian(lat, lon, 0, Cartesian::Units::DEGREES));
}


int main() {
	const double lons[] = {0, 10, 45, 89.5, 90, 91, 135, 179.5, 180, -179.5, -135, -90, -45, -10};
	const double lats[] = {-89.5, -60, -30, -10, -1e-6, 1e-6, 10, 30, 60, 89.5};

	// The equator at several longitudes, in degrees and as exact vectors with z = 0.
	for (double lon : lons) {
		check_degrees(0, lon);
		double a = lon * M_PI / 180;
		check("z = 0", Cartesian(std::cos(a), std::sin(a), 0.0));
	}

	// Meridians 0, 90, 180 and 270: in degrees (lon 0 lies on y = 0 exactly), and as
	// exact vectors on the planes y = 0 (lon 0 and 180) and x = 0 (lon 90 and 270).
	for (double lat : lats) {
		for (double lon : {0.0, 90.0, 180.0, -90.0}) {
			check_degrees(lat, lon);
		}
		double a = lat * M_PI / 180, c = std::cos(a), s = std::sin(a);
		check("lon 0, y = 0", Cartesian(c, 0.0, s));
		check("lon 90, x = 0", Cartesian(0.0, c, s));
		check("lon 180, y = 0", Cartesian(-c, 0.0, s));
		check("lon 270, x = 0", Cartesian(0.0, -c, s));
	}

	// The poles and the other root vertices, and the midpoints of root edges.
	check_degrees(90, 0);
	check_degrees(-90, 0);
	for (const auto& v : start_vertices) {
		check("root vertex", v);
	}
	for (const auto& t : start_trixels) {
		const auto& v0 = start_vertices[t.v0];
		const auto& v1 = start_vertices[t.v1];
		const auto& v2 = start_vertices[t.v2];
		check("root edge midpoint", HTM::midPoint(v0, v1));
		check("root edge midpoint", HTM::midPoint(v1, v2));
		check("root edge midpoint", HTM::midPoint(v2, v0));
	}

	std::printf("edge points: checked %zu, failures %zu\n", checked, failures);
	return failures ? 1 : 0;
}
