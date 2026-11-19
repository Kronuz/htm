// Prints the ids of N random points on the sphere (seeded), one per line, for
// test/compare_ids.sh to compare two commits of htm.cc.
//
// Usage: ids_dump [points]   (default 100,000)
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>

#include "htm.h"

int main(int argc, char** argv) {
	size_t n = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 100000;
	std::mt19937_64 rng(20261118);
	std::normal_distribution<double> g(0, 1);
	for (size_t i = 0; i < n; ++i) {
		double x = g(rng), y = g(rng), z = g(rng);
		double r = std::sqrt(x * x + y * y + z * z);
		Cartesian c(x / r, y / r, z / r);
		std::printf("%a %a %a %llu\n", c.x, c.y, c.z, (unsigned long long)HTM::getId(c));
	}
	return 0;
}
