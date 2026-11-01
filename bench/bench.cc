// Benchmark: HTM::getId(Cartesian) and HTM::getTrixelName(Cartesian) on random unit
// vectors, and, when built with HTM_BENCH_COMPARE / HTM_BENCH_S2, the same points through
// the SDSS HTM library (idByPoint, nameByPoint), H3 (latLngToCell) and Google S2
// (S2CellId). Every library receives its own input type, converted once before any
// timing; the timed region holds only the library call.
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Release -DHTM_BENCH_COMPARE=ON && cmake --build build --target htm_bench
// Run:   ./build/htm_bench [points [rounds]]
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "htm.h"

#ifdef HTM_BENCH_SDSS
#include "SpatialIndex.h"
#endif
#ifdef HTM_BENCH_H3
#include <h3api.h>
#endif
#ifdef HTM_BENCH_S2
#include "s2/s2cell_id.h"
#include "s2/s2latlng.h"
#include "s2/s2metrics.h"
#include "s2/s2point.h"
#endif

namespace {

constexpr double EARTH_RADIUS_M = 6371008.8;  // mean radius
constexpr int H3_RES = 15;                    // the finest H3 resolution
constexpr int S2_LEVEL = 25;                  // closest S2 cell area to an HTM level 25 trixel
constexpr size_t SDSS_CHECK_LEVEL = 14;       // levels every point must name identically

struct Row {
	std::string call;
	std::string lib;
	std::function<uint64_t()> run;
	std::vector<double> ns;
};

[[noreturn]] void fail(const char* what, size_t i) {
	std::fprintf(stderr, "htm_bench: %s (point %zu)\n", what, i);
	std::exit(1);
}

}  // namespace

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 20000;
	int rounds = argc > 2 ? std::atoi(argv[2]) : 5;
	std::mt19937_64 rng(42);
	std::normal_distribution<double> g;
	std::vector<Cartesian> pts;
	for (size_t i = 0; i < n; ++i) {
		double x = g(rng), y = g(rng), z = g(rng);
		pts.emplace_back(x, y, z);
		pts.back().normalize();
	}

	std::vector<Row> rows;
	volatile uint64_t sink = 0;
	auto add = [&](std::string call, std::string lib, std::function<uint64_t()> run) {
		rows.push_back({std::move(call), std::move(lib), std::move(run), {}});
	};

	add("getId", "htm", [&] { uint64_t s = 0; for (auto& p : pts) s += HTM::getId(p); return s; });
	add("getId", "htm (A/A)", [&] { uint64_t s = 0; for (auto& p : pts) s += HTM::getId(p); return s; });
	add("getTrixelName", "htm", [&] {
		uint64_t s = 0;
		for (auto& p : pts) { auto name = HTM::getTrixelName(p); s += name.size() + name.back(); }
		return s;
	});

	const double area_htm = 4 * M_PI * EARTH_RADIUS_M * EARTH_RADIUS_M / (8.0 * std::pow(4.0, HTM_MAX_LEVEL));
	const double edge_htm = EARTH_RADIUS_M * M_PI / 2 / std::pow(2.0, HTM_MAX_LEVEL);
	std::printf("cells: htm level %zu area %.4f m2 edge %.3f m (root edge / 2^level)\n", HTM_MAX_LEVEL, area_htm, edge_htm);

#ifdef HTM_BENCH_SDSS
	SpatialIndex sdss(HTM_MAX_LEVEL);
	std::vector<SpatialVector> sdss_pts;
	for (auto& p : pts) sdss_pts.emplace_back(p.x, p.y, p.z);
	{
		// The SDSS library names the same mesh: same root octants and child numbering. Its ids
		// differ in one bit (the N/S bit), and its fixed 1e-15 tolerance in the sign tests picks
		// a neighbouring trixel at the deepest levels, so the check covers the first
		// SDSS_CHECK_LEVEL levels of every name, and the id relation where the names are equal.
		size_t full = 0;
		char name[HTM_MAX_LENGTH_NAME + 1];
		for (size_t i = 0; i < n; ++i) {
			uint64_t id = sdss.idByPoint(sdss_pts[i]);
			SpatialIndex::nameById(id, name);
			std::string mine = HTM::getTrixelName(pts[i]);
			if (std::strncmp(name, mine.c_str(), SDSS_CHECK_LEVEL + 2) != 0) {
				std::fprintf(stderr, "ours %s sdss %s\n", mine.c_str(), name);
				fail("htm and SDSS name different trixels within the first levels", i);
			}
			if (mine == name) {
				++full;
				if ((HTM::getId(pts[i]) ^ (uint64_t{1} << (2 * HTM_MAX_LENGTH_NAME - 2))) != id) {
					fail("equal names, ids are not related by the N/S bit", i);
				}
			}
		}
		std::printf("sdss check: first %zu levels equal on %zu of %zu points; all %zu levels on %zu\n",
			SDSS_CHECK_LEVEL, n, n, HTM_MAX_LEVEL, full);
	}
	add("getId", "SDSS HTM", [&] { uint64_t s = 0; for (auto& v : sdss_pts) s += sdss.idByPoint(v); return s; });
	add("getTrixelName", "SDSS HTM", [&] {
		uint64_t s = 0;
		char name[HTM_MAX_LENGTH_NAME + 1];
		for (auto& v : sdss_pts) { sdss.nameByPoint(v, name); s += name[HTM_MAX_LENGTH_NAME - 1]; }
		return s;
	});
#endif

#ifdef HTM_BENCH_H3
	std::vector<LatLng> ll;
	for (auto& p : pts) ll.push_back({std::asin(p.z), std::atan2(p.y, p.x)});
	{
		// Different mesh, different ids: check that the cell found for each point is centred
		// within a metre of it.
		for (size_t i = 0; i < n; ++i) {
			H3Index cell = 0;
			LatLng centre;
			if (latLngToCell(&ll[i], H3_RES, &cell) != E_SUCCESS || cellToLatLng(cell, &centre) != E_SUCCESS) {
				fail("H3 failed", i);
			}
			double c = std::cos(ll[i].lat) * std::cos(centre.lat) * std::cos(ll[i].lng - centre.lng) + std::sin(ll[i].lat) * std::sin(centre.lat);
			if (std::acos(std::min(1.0, c)) * EARTH_RADIUS_M > 1.0) {
				fail("H3 cell centre is more than 1 m from its point", i);
			}
		}
		double area = 0, edge = 0;
		getHexagonAreaAvgM2(H3_RES, &area);
		getHexagonEdgeLengthAvgM(H3_RES, &edge);
		std::printf("cells: h3 resolution %d area %.4f m2 edge %.3f m (H3 averages)\n", H3_RES, area, edge);
	}
	add("getId", "H3 res 15", [&] {
		uint64_t s = 0;
		for (auto& l : ll) { H3Index cell; latLngToCell(&l, H3_RES, &cell); s += cell; }
		return s;
	});
#endif

#ifdef HTM_BENCH_S2
	std::vector<S2Point> s2_pts;
	for (auto& p : pts) s2_pts.emplace_back(p.x, p.y, p.z);
	{
		for (size_t i = 0; i < n; ++i) {
			S2CellId cell = S2CellId(s2_pts[i]).parent(S2_LEVEL);
			if (!cell.is_valid() || (cell.ToPoint() - s2_pts[i]).Norm() * EARTH_RADIUS_M > 1.0) {
				fail("S2 cell centre is more than 1 m from its point", i);
			}
		}
		std::printf("cells: s2 level %d area %.4f m2 edge %.3f m (S2 averages)\n", S2_LEVEL,
			S2::kAvgArea.GetValue(S2_LEVEL) * EARTH_RADIUS_M * EARTH_RADIUS_M, S2::kAvgEdge.GetValue(S2_LEVEL) * EARTH_RADIUS_M);
	}
	add("getId", "S2 level 25", [&] {
		uint64_t s = 0;
		for (auto& p : s2_pts) s += S2CellId(p).parent(S2_LEVEL).id();
		return s;
	});
#endif

	// Interleaved rounds: every row runs once per round, so drift hits all rows alike.
	for (int r = 0; r < rounds; ++r) {
		for (auto& row : rows) {
			auto t0 = std::chrono::steady_clock::now();
			uint64_t s = row.run();
			auto t1 = std::chrono::steady_clock::now();
			sink = sink + s;
			row.ns.push_back(std::chrono::duration<double, std::nano>(t1 - t0).count() / n);
		}
	}

	std::printf("points %zu, %d interleaved rounds; ns per call\n", n, rounds);
	std::printf("%-14s %-12s %9s %9s %9s\n", "call", "library", "best", "median", "worst");
	for (auto& row : rows) {
		std::sort(row.ns.begin(), row.ns.end());
		std::printf("%-14s %-12s %9.1f %9.1f %9.1f\n", row.call.c_str(), row.lib.c_str(), row.ns.front(), row.ns[row.ns.size() / 2], row.ns.back());
	}
	return 0;
}
