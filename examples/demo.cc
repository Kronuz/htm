// A runnable tour of htm.
//
// Build (when this repo is the top-level project):
//   cmake -B build && cmake --build build && ./build/htm_demo
//
// The one idea worth taking away: the Hierarchical Triangular Mesh assigns every
// point on the sphere a trixel name (an "N"/"S" root plus a string of 0..3
// digits, one per subdivision level) and a matching integer id. Nearby points
// share a prefix, so the name is also a spatial key: trim digits to zoom out,
// and the id ranges of a trixel cover its whole subtree.
#include <cstdio>
#include <string>
#include <tuple>

#include "htm.h"

static void rule(const char* title) {
	std::printf("\n\033[1m── %s ──\033[0m\n", title);
}

// Name and id for a real lat/lon point.
static void locate(const char* label, double lat, double lon) {
	Cartesian c(lat, lon, 0.0, Cartesian::Units::DEGREES);
	std::string name = HTM::getTrixelName(c);
	std::printf("  %-16s lat=%9.4f lon=%9.4f  ->  %s  (id %llu)\n",
		label, lat, lon, name.c_str(),
		static_cast<unsigned long long>(HTM::getId(c)));
}

int main() {
	std::puts("htm demo");

	// --- 1. name a couple of real places --------------------------------------
	rule("every lat/lon point gets a trixel name and id");
	// Each name is "N"/"S" + a root digit + one digit per level down to the max
	// depth. Both places fall in different parts of the mesh.
	locate("San Francisco", 37.7749, -122.4194);
	locate("Sydney", -33.8688, 151.2093);
	locate("Null Island", 0.0, 0.0);

	// --- 2. the name is a spatial prefix --------------------------------------
	rule("trimming digits zooms out; the prefix is the enclosing trixel");
	// Sydney's full name and its level-2 ancestor (root + 2 digits).
	Cartesian syd(-33.8688, 151.2093, 0.0, Cartesian::Units::DEGREES);
	std::string full = HTM::getTrixelName(syd);
	std::string coarse = full.substr(0, 4);  // "S" + root + two levels
	std::printf("  full   : %s\n", full.c_str());
	std::printf("  coarse : %s   (the trixel that contains it a few levels up)\n",
		coarse.c_str());

	// --- 3. id ranges cover a whole subtree -----------------------------------
	rule("a trixel's id range covers everything beneath it");
	// The coarse ancestor's range brackets the leaf id, since the leaf lives
	// inside that subtree.
	auto r = HTM::getRange(coarse);
	std::printf("  range(%s) = %llu .. %llu\n", coarse.c_str(),
		static_cast<unsigned long long>(r.start),
		static_cast<unsigned long long>(r.end));
	std::printf("  id(%s)    = %llu  (inside the range above)\n", full.c_str(),
		static_cast<unsigned long long>(HTM::getId(full)));

	// --- 4. the corners of a trixel -------------------------------------------
	rule("each trixel is a spherical triangle: getCorners gives its 3 vertices");
	auto [v0, v1, v2] = HTM::getCorners(std::string("N3"));
	std::printf("  N3 corners: (%g, %g, %g)  (%g, %g, %g)  (%g, %g, %g)\n",
		v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);

	std::puts("\ndone.");
	return 0;
}
