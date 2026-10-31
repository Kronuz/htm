// Benchmark: HTM::getId / getTrixelName(Cartesian) / getTrixelName(id) against
// the reference implementation (test/frozen.h) on random unit vectors.
//
// Build: cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --target htm_bench
// Run:   ./build/htm_bench [points]
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

#include "../test/frozen.h"
#include "htm.h"

template <typename F>
static double best_ns(size_t n, F&& f) {
	double best = 1e300;
	for (int rep = 0; rep < 7; ++rep) {
		auto t0 = std::chrono::steady_clock::now();
		f();
		auto t1 = std::chrono::steady_clock::now();
		best = std::min(best, std::chrono::duration<double, std::nano>(t1 - t0).count() / n);
	}
	return best;
}

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 20000;
	std::mt19937_64 rng(42);
	std::normal_distribution<double> g;
	std::vector<Cartesian> pts;
	std::vector<uint64_t> ids;
	for (size_t i = 0; i < n; ++i) {
		double x = g(rng), y = g(rng), z = g(rng);
		pts.emplace_back(x, y, z);
		pts.back().normalize();
		ids.push_back(HTM::getId(pts.back()));
	}
	volatile uint64_t sink = 0;
	auto run = [&](auto&& fn) { return best_ns(n, [&] { uint64_t s = 0; for (auto& p : pts) s += fn(p); sink = sink + s; }); };
	auto runi = [&](auto&& fn) { return best_ns(n, [&] { uint64_t s = 0; for (auto id : ids) s += fn(id); sink = sink + s; }); };
	std::printf("getId(Cartesian)       frozen %8.1f ns   new %8.1f ns\n",
		run([](const Cartesian& p) { return frozen_getId(p); }),
		run([](const Cartesian& p) { return HTM::getId(p); }));
	std::printf("getTrixelName(Cartes.) frozen %8.1f ns   new %8.1f ns\n",
		run([](const Cartesian& p) { return frozen_getTrixelName(p).size(); }),
		run([](const Cartesian& p) { return HTM::getTrixelName(p).size(); }));
	std::printf("getTrixelName(id)      (unchanged) frozen %8.1f ns   new %8.1f ns\n",
		runi([](uint64_t id) { return frozen_name_from_id(id).size(); }),
		runi([](uint64_t id) { return HTM::getTrixelName(id).size(); }));
	return 0;
}
