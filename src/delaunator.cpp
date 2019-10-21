#include "delaunator.hpp"

using namespace Terra;

// Kahan and Babuska summation, Neumaier variant; accumulates less FP error
inline double sum(const std::vector<double>& x)
{
    double sum = x[0];
    double err = 0.0;

    for (size_t i = 1; i < x.size(); i++)
    {
        const double k = x[i];
        const double m = sum + k;
        err += std::fabs(sum) >= std::fabs(k) ? sum - m + k : k - m + sum;
        sum = m;
    }

    return sum + err;
}

inline double dist(const Terra::Vector2& a, const Terra::Vector2& b)
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;

    return dx * dx + dy * dy;
}

inline double circumradius(const Terra::Vector2& a, const Terra::Vector2& b, const Terra::Vector2& c)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double ex = c.x - a.x;
    const double ey = c.y - a.y;

    const double bl = dx * dx + dy * dy;
    const double cl = ex * ex + ey * ey;
    const double d = dx * ey - dy * ex;

    const double x = (ey * bl - dy * cl) * 0.5 / d;
    const double y = (dx * cl - ex * bl) * 0.5 / d;

    if ((bl > 0.0 || bl < 0.0) && (cl > 0.0 || cl < 0.0) && (d > 0.0 || d < 0.0))
    {
        return x * x + y * y;
    }
    else
    {
        return std::numeric_limits<double>::max();
    }
}

inline bool orient(const Terra::Vector2& p, const Terra::Vector2& q, const Terra::Vector2& r)
{
    return (q.y - p.y) * (r.x - q.x) - (q.x - p.x) * (r.y - q.y) < 0.0;
}

inline Terra::Vector2 circumcentre( const Terra::Vector2& a, const Terra::Vector2& b, const Terra::Vector2& c)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double ex = c.x - a.x;
    const double ey = c.y - a.y;

    const double bl = dx * dx + dy * dy;
    const double cl = ex * ex + ey * ey;
    const double d = dx * ey - dy * ex;

    const double x = a.x + (ey * bl - dy * cl) * 0.5 / d;
    const double y = a.y + (dx * cl - ex * bl) * 0.5 / d;

    return Terra::Vector2(x, y);
}

struct compare
{
    const std::vector<Terra::Vector2>& coords;
    Terra::Vector2 c;

    bool operator()(size_t i, size_t j)
    {
        const Terra::Vector2& iv = coords[i];
        const Terra::Vector2& jv = coords[j];

        const double d1 = dist(iv, c);
        const double d2 = dist(jv, c);

        const double diff1 = d1 - d2;
        const double diff2 = iv.x - jv.x;
        const double diff3 = iv.y - jv.y;

        if (diff1 > 0.0 || diff1 < 0.0)
        {
            return diff1 < 0;
        }
        else if (diff2 > 0.0 || diff2 < 0.0)
        {
            return diff2 < 0;
        }
        else
        {
            return diff3 < 0;
        }
    }
};

inline bool in_circle(const Terra::Vector2& a, const Terra::Vector2& b, const Terra::Vector2& c, const Terra::Vector2& p)
{
    const double dx = a.x - p.x;
    const double dy = a.y - p.y;
    const double ex = b.x - p.x;
    const double ey = b.y - p.y;
    const double fx = c.x - p.x;
    const double fy = c.y - p.y;

    const double ap = dx * dx + dy * dy;
    const double bp = ex * ex + ey * ey;
    const double cp = fx * fx + fy * fy;

    return (dx * (ey * cp - bp * fy) -
            dy * (ex * cp - bp * fx) +
            ap * (ex * fy - ey * fx)) < 0.0;
}

inline bool check_pts_equal(const Terra::Vector2& a, const Terra::Vector2& b)
{
    return std::fabs(a.x - b.x) <= EPSILON &&
           std::fabs(a.y - b.y) <= EPSILON;
}

// monotonically increases with real angle, but doesn't need expensive trigonometry
inline double pseudo_angle(const double dx, const double dy)
{
    const double p = dx / (std::abs(dx) + std::abs(dy));

    return (dy > 0.0 ? 3.0 - p : 1.0 + p) / 4.0; // [0..1)
}

Delaunator::Delaunator(const std::vector<Terra::Vector2>& in_coords) :
    coords(in_coords),
    triangles(),
    halfedges(),
    hull_prev(),
    hull_next(),
    hull_tri(),
    hull_start(),
    m_hash(),
    m_hash_size(),
    m_edge_stack()
{
    size_t n = coords.size();

    double max_x = std::numeric_limits<double>::min();
    double max_y = std::numeric_limits<double>::min();
    double min_x = std::numeric_limits<double>::max();
    double min_y = std::numeric_limits<double>::max();
    std::vector<size_t> ids;
    ids.reserve(n);

    for (size_t i = 0; i < n; i++)
    {
        auto p = coords[2 * i];
        const double x = p.x;
        const double y = p.y;

        if (x < min_x) min_x = x;
        if (y < min_y) min_y = y;
        if (x > max_x) max_x = x;
        if (y > max_y) max_y = y;

        ids.push_back(i);
    }
    
    const auto c = Terra::Vector2((min_x + max_x) / 2, (min_y + max_y) / 2);
    double min_dist = std::numeric_limits<double>::max();

    size_t i0 = INVALID_INDEX;
    size_t i1 = INVALID_INDEX;
    size_t i2 = INVALID_INDEX;

    // pick a seed point close to the centroid
    for (size_t i = 0; i < n; i++)
    {
        const double d = dist(c, coords[i]);
        if (d < min_dist)
        {
            i0 = i;
            min_dist = d;
        }
    }

    auto i0v = coords[i0];

    min_dist = std::numeric_limits<double>::max();

    // find the point closest to the seed
    for (size_t i = 0; i < n; i++)
    {
        if (i == i0) continue;
        const double d = dist(i0v, coords[i]);
        if (d < min_dist && d > 0.0)
        {
            i1 = i;
            min_dist = d;
        }
    }

    auto i1v = coords[i1];

    double min_radius = std::numeric_limits<double>::max();

    // find the third point which forms the smallest circumcircle with the first two
    for (size_t i = 0; i < n; i++) {
        if (i == i0 || i == i1) continue;

        const double r = circumradius(i0v, i1v, coords[i]);
        if (r < min_radius)
        {
            i2 = i;
            min_radius = r;
        }
    }

    if (!(min_radius < std::numeric_limits<double>::max()))
    {
        throw std::runtime_error("not triangulation");
    }

    auto i2v = coords[i2];

    if (orient(i0, i1, i2))
    {
        std::swap(i1, i2);
        std::swap(i1v, i2v);
    }

    m_centre = circumcentre(i0, i1, i2);

    // sort the points by distance from the seed triangle circumcentre
    std::sort(ids.begin(), ids.end(), compare{ coords, m_centre });

    // initialize a hash table for storing edges of the advancing convex hull
    m_hash_size = static_cast<size_t>(std::llround(std::ceil(std::sqrt(n))));
    m_hash.resize(m_hash_size);
    std::fill(m_hash.begin(), m_hash.end(), INVALID_INDEX);

    // initialize arrays for tracking the edges of the advancing convex hull
    hull_prev.resize(n);
    hull_next.resize(n);
    hull_tri.resize(n);

    hull_start = i0;

    size_t hull_size = 3;

    hull_next[i0] = hull_prev[i2] = i1;
    hull_next[i1] = hull_prev[i0] = i2;
    hull_next[i2] = hull_prev[i1] = i0;

    hull_tri[i0] = 0;
    hull_tri[i1] = 1;
    hull_tri[i2] = 2;

    m_hash[hash_key(i0v)] = i0;
    m_hash[hash_key(i1v)] = i1;
    m_hash[hash_key(i2v)] = i2;

    size_t max_triangles = n < 3 ? 1 : 2 * n - 5;
    triangles.reserve(max_triangles * 3);
    halfedges.reserve(max_triangles * 3);
    add_triangle(i0, i1, i2, INVALID_INDEX, INVALID_INDEX, INVALID_INDEX);
    auto last_point = Terra::Vector2
    (
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN()
    );
    for (size_t k = 0; k < n; k++)
    {
        const size_t i = ids[k];
        const auto current_point = coords[i];

        // skip near-duplicate points
        if (k > 0 && check_pts_equal(current_point, last_point)) continue;
        last_point = current_point;

        // skip seed triangle points
        if (
            check_pts_equal(current_point, i0v) ||
            check_pts_equal(current_point, i1v) ||
            check_pts_equal(current_point, i2v)) continue;

        // find a visible edge on the convex hull using edge hash
        size_t start = 0;

        size_t key = hash_key(current_point);
        for (size_t j = 0; j < m_hash_size; j++)
        {
            start = m_hash[Utils::FastMod<size_t>(key + j, m_hash_size)];
            if (start != INVALID_INDEX && start != hull_next[start]) break;
        }

        start = hull_prev[start];
        size_t e = start;
        size_t q;

        //TODO: does it works in a same way as in JS
        while (q = hull_next[e], !orient(current_point, coords[e], coords[q]))
        {
            e = q;
            if (e == start) {
                e = INVALID_INDEX;
                break;
            }
        }

        if (e == INVALID_INDEX) continue; // likely a near-duplicate point; skip it

        // add the first triangle from the point
        size_t t = add_triangle
        (
            e,
            i,
            hull_next[e],
            INVALID_INDEX,
            INVALID_INDEX,
            hull_tri[e]
        );

        hull_tri[i] = legalize(t + 2);
        hull_tri[e] = t;
        hull_size++;

        // walk forward through the hull, adding more triangles and flipping recursively
        size_t next = hull_next[e];
        while
        (
            q = hull_next[next],
            orient(current_point, coords[next], coords[q])
        )
        {
            t = add_triangle(next, i, q, hull_tri[i], INVALID_INDEX, hull_tri[next]);
            hull_tri[i] = legalize(t + 2);
            hull_next[next] = next; // mark as removed
            hull_size--;
            next = q;
        }

        // walk backward from the other side, adding more triangles and flipping
        if (e == start)
        {
            while
            (
                q = hull_prev[e],
                orient(current_point, coords[q], coords[e])
            )
            {
                t = add_triangle(q, i, e, INVALID_INDEX, hull_tri[e], hull_tri[q]);
                legalize(t + 2);
                hull_tri[q] = t;
                hull_next[e] = e; // mark as removed
                hull_size--;
                e = q;
            }
        }

        // update the hull indices
        hull_prev[i] = e;
        hull_start = e;
        hull_prev[next] = i;
        hull_next[e] = i;
        hull_next[i] = next;

        m_hash[hash_key(current_point)] = i;
        m_hash[hash_key(coords[e])] = e;
    }
}

double Delaunator::get_hull_area()
{
    std::vector<double> hull_area;
    size_t e = hull_start;
    do
    {
        const auto ev = coords[e];
        const auto pv = coords[hull_prev[e]];

        hull_area.push_back((ev.x - pv.x) * (ev.y - pv.y));
        e = hull_next[e];
    } while (e != hull_start);

    return sum(hull_area);
}

size_t Delaunator::legalize(size_t a)
{
    size_t i = 0;
    size_t ar = 0;
    m_edge_stack.clear();

    // recursion eliminated with a fixed-size stack
    while (true) {
        const size_t b = halfedges[a];

        /* if the pair of triangles doesn't satisfy the Delaunay condition
        * (p1 is inside the circumcircle of [p0, pl, pr]), flip them,
        * then do the same check/flip recursively for the new pair of triangles
        *
        *           pl                    pl
        *          /||\                  /  \
        *       al/ || \bl            al/    \a
        *        /  ||  \              /      \
        *       /  a||b  \    flip    /___ar___\
        *     p0\   ||   /p1   =>   p0\---bl---/p1
        *        \  ||  /              \      /
        *       ar\ || /br             b\    /br
        *          \||/                  \  /
        *           pr                    pr
        */
        const size_t a0 = 3 * (a / 3);
        ar = a0 + (a + 2) % 3;

        if (b == INVALID_INDEX) {
            if (i > 0) {
                i--;
                a = m_edge_stack[i];
                continue;
            } else {
                //i = INVALID_INDEX;
                break;
            }
        }

        const size_t b0 = 3 * (b / 3);
        const size_t al = a0 + (a + 1) % 3;
        const size_t bl = b0 + (b + 2) % 3;

        const size_t p0 = triangles[ar];
        const size_t pr = triangles[a];
        const size_t pl = triangles[al];
        const size_t p1 = triangles[bl];

        const bool illegal = in_circle
        (
            coords[p0],
            coords[pr],
            coords[pl],
            coords[p1]
        );

        if (illegal) {
            triangles[a] = p1;
            triangles[b] = p0;

            auto hbl = halfedges[bl];

            // edge swapped on the other side of the hull (rare); fix the halfedge reference
            if (hbl == INVALID_INDEX) {
                size_t e = hull_start;
                do {
                    if (hull_tri[e] == bl) {
                        hull_tri[e] = a;
                        break;
                    }
                    e = hull_next[e];
                } while (e != hull_start);
            }
            link(a, hbl);
            link(b, halfedges[ar]);
            link(ar, bl);
            size_t br = b0 + (b + 1) % 3;

            if (i < m_edge_stack.size()) {
                m_edge_stack[i] = br;
            } else {
                m_edge_stack.push_back(br);
            }
            i++;

        } else {
            if (i > 0) {
                i--;
                a = m_edge_stack[i];
                continue;
            } else {
                break;
            }
        }
    }
    return ar;
}

inline size_t Delaunator::hash_key(const Terra::Vector2& vec) const
{
    const double dx = vec.x - m_centre.x;
    const double dy = vec.y - m_centre.y;
    return Utils::FastMod<size_t>
    (
        static_cast<size_t>(std::llround(std::floor(pseudo_angle(dx, dy) * static_cast<double>(m_hash_size)))),
        m_hash_size
    );
}

size_t Delaunator::add_triangle(size_t i0, size_t i1, size_t i2, size_t a, size_t b, size_t c)
{
    size_t t = triangles.size();
    triangles.push_back(i0);
    triangles.push_back(i1);
    triangles.push_back(i2);
    link(t, a);
    link(t + 1, b);
    link(t + 2, c);

    return t;
}

void Delaunator::link(const size_t a, const size_t b)
{
    size_t s = halfedges.size();
    if (a == s)
    {
        halfedges.push_back(b);
    }
    else if (a < s)
    {
        halfedges[a] = b;
    }
    else
    {
        throw std::runtime_error("Cannot link edge");
    }
    if (b != INVALID_INDEX)
    {
        size_t s2 = halfedges.size();
        if (b == s2)
        {
            halfedges.push_back(a);
        }
        else if (b < s2)
        {
            halfedges[b] = a;
        }
        else
        {
            throw std::runtime_error("Cannot link edge");
        }
    }
}