// Smoke test for the standalone htm library.
//
// Exercises the real HTM core: trixel naming and id for known Cartesian
// directions, the name <-> id round trip, getCorners() of start trixels,
// getRange(), the trixel-set boolean operations (union / intersection /
// exclusive disjunction), the Constraint spherical-cap primitive, and the
// Constraint-based ranging helpers (bounding circle, inside test).
//
// All expected values were derived by tracing the code and confirmed against a
// build, not guessed.
//
// Build via CMake: cmake -B build && cmake --build build && ctest --test-dir build
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include "htm.h"


// The eight start trixels: ids 8..15 map to N0..N3, S0..S3.
static const char* kStartNames[8] = {"N0", "N1", "N2", "N3", "S0", "S1", "S2", "S3"};

// A full-depth (HTM_MAX_LEVEL == 25) name is 'N'/'S' + 1 root digit + 25 digits.
static const size_t kFullLen = HTM_MAX_LENGTH_NAME;  // 27


// ---------------------------------------------------------------------------
// startTrixel + getTrixelName / getId: a Cartesian direction lands in the
// expected root trixel, and getId(coord) agrees with getId(name).
// ---------------------------------------------------------------------------

static void test_naming_and_id() {
	struct { double x, y, z; const char* root; uint64_t id; } cases[] = {
		// startTrixel routes by the octant of (x, y, z).
		{ 1.0,  0.0,  0.0, "N3", 13510798882111487ull},  // +X, z>=0 -> N3
		{ 0.0,  0.0,  1.0, "N3", 13510798882111487ull},  // +Z pole -> N3
		{ 1.0,  1.0,  1.0, "N3", 13510798882111487ull},  // first octant -> N3
		{-1.0,  0.0,  0.0, "N1", 11258999068426239ull},  // -X -> N1
		{ 0.0,  1.0,  0.0, "N2", 12384898975268863ull},  // +Y -> N2
		{ 0.0, -1.0,  0.0, "N0", 10133099161583615ull},  // -Y -> N0
		{ 0.0,  0.0, -1.0, "S0", 14636698788954111ull},  // -Z pole -> S0
		{-1.0, -1.0, -1.0, "S2", 16888498602639359ull},  // last octant -> S2
	};

	for (const auto& c : cases) {
		Cartesian coord(c.x, c.y, c.z);
		std::string name = HTM::getTrixelName(coord);

		// A full-depth name with the expected two-character root.
		assert(name.length() == kFullLen);
		assert(name[0] == c.root[0] && name[1] == c.root[1]);

		// getId(coord) matches the precomputed id and agrees with getId(name).
		uint64_t id = HTM::getId(coord);
		assert(id == c.id);
		assert(HTM::getId(name) == id);
	}

	std::printf("htm naming/id OK: directions route to expected root trixels\n");
}


// ---------------------------------------------------------------------------
// Real lat/lon points produce their known trixel name and id.
// ---------------------------------------------------------------------------

static void test_latlon_points() {
	// lat = 45, lon = 45.
	Cartesian p45(45.0, 45.0, 0.0, Cartesian::Units::DEGREES);
	assert(HTM::getTrixelName(p45) == "N33313330300033330330300333");
	assert(HTM::getId(p45) == 13475391439359039ull);

	// Sydney (lat = -33.8688, lon = 151.2093).
	Cartesian syd(-33.8688, 151.2093, 0.0, Cartesian::Units::DEGREES);
	assert(HTM::getTrixelName(syd) == "S13011231030232202122301131");
	assert(HTM::getId(syd) == 15506219993771101ull);

	std::printf("htm lat/lon OK: 45/45 and Sydney name+id match\n");
}


// ---------------------------------------------------------------------------
// name <-> id round trip on the start ids and on a deep name.
// ---------------------------------------------------------------------------

static void test_name_id_roundtrip() {
	// The eight raw start ids 8..15 print as N0..N3, S0..S3.
	for (uint64_t i = 8; i <= 15; ++i) {
		std::string name = HTM::getTrixelName(i);
		assert(name == kStartNames[i - 8]);
		assert(HTM::getId(name) == i);
	}

	// getId of the short root names is the documented value.
	assert(HTM::getId(std::string("N3")) == 11ull);
	assert(HTM::getId(std::string("S0")) == 12ull);

	// A full-depth name round-trips through id and back unchanged.
	Cartesian syd(-33.8688, 151.2093, 0.0, Cartesian::Units::DEGREES);
	std::string name = HTM::getTrixelName(syd);
	uint64_t id = HTM::getId(name);
	assert(HTM::getTrixelName(id) == name);

	std::printf("htm round trip OK: start ids and a deep name survive id<->name\n");
}


// ---------------------------------------------------------------------------
// getCorners returns three unit-ish vertices, and rejects a bad name.
// ---------------------------------------------------------------------------

static void test_corners() {
	// N3 is the start trixel { 2, 0, 1 } over start_vertices, i.e. corners
	// (0,1,0), (0,0,1), (1,0,0).
	auto [a, b, c] = HTM::getCorners(std::string("N3"));
	assert(a.x == 0.0 && a.y == 1.0 && a.z == 0.0);
	assert(b.x == 0.0 && b.y == 0.0 && b.z == 1.0);
	assert(c.x == 1.0 && c.y == 0.0 && c.z == 0.0);

	// A deeper trixel still yields three corners on the unit sphere.
	auto [d, e, f] = HTM::getCorners(std::string("N30"));
	for (const auto& v : {d, e, f}) {
		double n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		assert(std::abs(n - 1.0) < 1e-9);
	}

	// An invalid digit (only 0..3 valid past the root) throws.
	bool threw = false;
	try {
		HTM::getCorners(std::string("N39"));
	} catch (const std::invalid_argument&) {
		threw = true;
	}
	assert(threw);

	std::printf("htm getCorners OK: three vertices, bad name throws\n");
}


// ---------------------------------------------------------------------------
// getRange: a root trixel covers a wide id range; a full-depth name is a point.
// ---------------------------------------------------------------------------

static void test_range() {
	// getRange("N3") spans the whole subtree of N3 at the max level.
	auto r = HTM::getRange(std::string("N3"));
	assert(r.start == 12384898975268864ull);
	assert(r.end == 13510798882111487ull);
	assert(r.start < r.end);

	// A full-depth name is a single id, so its range is degenerate (start==end).
	Cartesian syd(-33.8688, 151.2093, 0.0, Cartesian::Units::DEGREES);
	std::string name = HTM::getTrixelName(syd);
	auto rp = HTM::getRange(name);
	assert(rp.start == rp.end);
	assert(rp.start == HTM::getId(name));

	// range_t::to_string uses std::format under the hood.
	assert(range_t(3, 7).to_string() == "3-7");

	std::printf("htm getRange OK: root spans a subtree, leaf is a point\n");
}


// ---------------------------------------------------------------------------
// Trixel-set boolean operations over sorted trixel-string vectors.
// ---------------------------------------------------------------------------

static void test_set_operations() {
	auto a = []() { return std::vector<std::string>{"N00", "N01", "N3"}; };
	auto b = []() { return std::vector<std::string>{"N01", "N02", "S0"}; };

	// Union is a sorted merge of both inputs (duplicates are kept).
	auto u = HTM::trixel_union(a(), b());
	assert((u == std::vector<std::string>{"N00", "N01", "N01", "N02", "N3", "S0"}));

	// Intersection keeps only trixels common to both (here, N01).
	auto inter = HTM::trixel_intersection(a(), b());
	assert((inter == std::vector<std::string>{"N01"}));

	// Exclusive disjunction keeps what is in exactly one side.
	auto xd = HTM::trixel_exclusive_disjunction(a(), b());
	assert((xd == std::vector<std::string>{"N00", "N02", "N3", "S0"}));

	// Intersection is prefix-aware: an ancestor on one side keeps the descendant
	// on the other ("N0" contains "N00").
	std::vector<std::string> anc{"N0"};
	std::vector<std::string> desc{"N00", "N3"};
	auto pinter = HTM::trixel_intersection(std::move(anc), std::move(desc));
	assert((pinter == std::vector<std::string>{"N00"}));

	std::printf("htm set ops OK: union/intersection/xor and prefix-aware intersect\n");
}


// ---------------------------------------------------------------------------
// Constraint (spherical cap) + Constraint-based ranging helpers.
// ---------------------------------------------------------------------------

static void test_constraint_and_ranging() {
	Cartesian px(1.0, 0.0, 0.0);

	// A 1 km cap around the +X point: a small positive half-space.
	Constraint cap(px, 1000.0);
	assert(cap.radius == 1000.0);
	assert(cap.sign == Constraint::Sign::POS);
	assert(cap.distance > 0.0);
	// arcangle is radius / earth-radius, a small angle.
	assert(cap.arcangle > 0.0 && cap.arcangle < 0.01);

	// The cap's own (normalized) center is inside the cap.
	Cartesian center = px;
	center.normalize();
	assert(HTM::insideVertex_Constraint(center, cap));

	// The default single-argument Constraint is a great circle: a half-space
	// through the origin, distance 0, sign ZERO.
	Constraint great(px);
	assert(great.sign == Constraint::Sign::ZERO);
	assert(std::abs(great.distance) < 1e-12);

	// getBoundingCircle of the N3 start trixel: its center is the normalized
	// (1,1,1) direction and the arcangle is the half-aperture of the cap.
	auto [v0, v1, v2] = HTM::getCorners(std::string("N3"));
	Constraint bc = HTM::getBoundingCircle(v0, v1, v2);
	double inv = 1.0 / std::sqrt(3.0);
	assert(std::abs(bc.center.x - inv) < 1e-9);
	assert(std::abs(bc.center.y - inv) < 1e-9);
	assert(std::abs(bc.center.z - inv) < 1e-9);
	assert(bc.arcangle > 0.9 && bc.arcangle < 1.0);  // ~0.9553 rad

	// All three trixel corners sit on the bounding circle (inside-or-on the cap).
	for (const auto& v : {v0, v1, v2}) {
		// On the boundary the dot product equals cos(arcangle); allow tolerance.
		assert(v * bc.center >= std::cos(bc.arcangle) - 1e-9);
	}

	std::printf("htm constraint/ranging OK: cap, great circle, bounding circle\n");
}


int main() {
	test_naming_and_id();
	test_latlon_points();
	test_name_id_roundtrip();
	test_corners();
	test_range();
	test_set_operations();
	test_constraint_and_ranging();
	std::printf("all htm tests passed\n");
	return 0;
}
