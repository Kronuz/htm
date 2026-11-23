// The root mesh tables: start_vertices holds the six axis points, start_trixels the
// eight root trixels, with the values the tables had as per-TU statics.
//
// Usage: htm_start_tables_test
#include <cstdio>

#include "htm.h"


static size_t failures = 0;


#define CHECK(cond)                                                          \
	do {                                                                     \
		if (!(cond)) {                                                       \
			std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                      \
		}                                                                    \
	} while (0)


int main() {
	static const double vertices[6][3] = {
		{ 0.0, 0.0, 1.0 }, { 1.0, 0.0, 0.0 }, { 0.0, 1.0, 0.0 },
		{ -1.0, 0.0, 0.0 }, { 0.0, -1.0, 0.0 }, { 0.0, 0.0, -1.0 },
	};
	for (int i = 0; i < 6; ++i) {
		CHECK(start_vertices[i].x == vertices[i][0]);
		CHECK(start_vertices[i].y == vertices[i][1]);
		CHECK(start_vertices[i].z == vertices[i][2]);
		CHECK(start_vertices[i].scale == 1);
		CHECK(start_vertices[i].getSRID() == WGS84);
	}

	struct expect_t {
		uint64_t id;
		const char* name;
		int v0, v1, v2;
	};
	static const expect_t trixels[8] = {
		{ 8, "N0", 1, 0, 4 }, { 9, "N1", 4, 0, 3 }, { 10, "N2", 3, 0, 2 }, { 11, "N3", 2, 0, 1 },
		{ 12, "S0", 1, 5, 2 }, { 13, "S1", 2, 5, 3 }, { 14, "S2", 3, 5, 4 }, { 15, "S3", 4, 5, 1 },
	};
	for (int i = 0; i < 8; ++i) {
		CHECK(start_trixels[i].id == trixels[i].id);
		CHECK(start_trixels[i].name == trixels[i].name);
		CHECK(start_trixels[i].v0 == trixels[i].v0);
		CHECK(start_trixels[i].v1 == trixels[i].v1);
		CHECK(start_trixels[i].v2 == trixels[i].v2);
	}

	if (failures) {
		std::fprintf(stderr, "%zu failures\n", failures);
		return 1;
	}
	std::printf("start tables ok\n");
	return 0;
}
