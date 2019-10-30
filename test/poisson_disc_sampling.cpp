#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <iostream>
#include <vector>

#include <catch2/catch.hpp>

#include "../src/types/vector_2.hpp"
#include "../src/types/hash_grid.hpp"
#include "../src/grid/poisson_disc_sampler.hpp"

TEST_CASE("can sample points correctly", "[PoissonDiscSampler]")
{
	const int64_t size_x = 1000;
	const int64_t size_y = 500;
	const double radius = 10.0;

	std::vector<Terra::Vector2> points;
	Terra::HashGrid grid(size_x, size_y, radius);

    Terra::Grid::PoissonDiscSampler sampler(points, grid, size_x, size_y, radius, 100);

	REQUIRE(sampler.Sample() >= 2500);
}