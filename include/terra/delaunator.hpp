#pragma once

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "types/vec2.hpp"
#include "math/fast_mod.hpp"

namespace terra
{
    constexpr double EPSILON = std::numeric_limits<double>::epsilon();
    constexpr size_t INVALID_INDEX = std::numeric_limits<size_t>::max();

    struct delaunator_point
    {
        size_t i;
        double x;
        double y;
        size_t t;
        size_t prev;
        size_t next;
        bool removed;
    };

    class delaunator
    {
       public:
        const std::vector<terra::vec2>& coords;
        std::vector<size_t> triangles;
        std::vector<size_t> halfedges;
        std::vector<size_t> hull_prev;
        std::vector<size_t> hull_next;
        std::vector<size_t> hull_tri;
        size_t hull_start;

        delaunator(const std::vector<terra::vec2>& in_coords);

        double get_hull_area();

       private:
        std::vector<size_t> m_hash;
        terra::vec2 m_centre;
        size_t m_hash_size;
        std::vector<size_t> m_edge_stack;

        size_t legalize(size_t a);
        size_t hash_key(const terra::vec2& vec) const;
        size_t add_triangle(size_t i0,
                            size_t i1,
                            size_t i2,
                            size_t a,
                            size_t b,
                            size_t c);
        void link(size_t a, size_t b);
    };
} // namespace terra
