#pragma once

#include <list>
#include <vector>

#include "../../enums/graph_type.hpp"
#include "graph_edge_t.hpp"

namespace terra::detail
{
    template<graph_type T>
    class graph_t
    {
    public:
        typedef graph_edge_t<T> edge;

        graph_t();
        graph_t(size_t node_count, size_t max_edges);
        graph_t(size_t node_count, const std::vector<size_t>& triangles);

        void add_edge(const edge& edge);
        void add_edge(size_t v0, size_t v1);
        void add_triangle(size_t v0, size_t v1, size_t v2);

        std::vector<size_t> get_connected(size_t i) const;

        size_t num_edges() const;

    private:
        std::vector<std::list<size_t>> nodes;
        std::vector<edge> edges;

        bool is_edge_duplicate(const edge& edge) const;
    };
} // namespace terra::detail