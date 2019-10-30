#include "types/hash_grid.hpp"

using namespace Terra;

HashGrid::HashGrid() : 
    gridSizeX(-1),
    gridSizeY(-1),
    neighbours(0),
    bucketSize(0.0),
    hashtable()
{
}

HashGrid::HashGrid(int64_t sizeX, int64_t sizeY, double radius)
{
    static const double sqrt1_2 = 1 / Utils::FastSqrt(2.0);

    this->bucketSize = radius * sqrt1_2;
    this->gridSizeX = Utils::FastCeil<int64_t>(static_cast<double>(sizeX) / bucketSize);
    this->gridSizeY = Utils::FastCeil<int64_t>(static_cast<double>(sizeY) / bucketSize);
    this->neighbours = Utils::FastCeil<int64_t>(radius / this->bucketSize);

    this->hashtable.reserve(this->gridSizeX * this->gridSizeY);
    std::fill(this->hashtable.begin(), this->hashtable.end(), -1);
}

constexpr size_t HashPos(const Terra::Vector2& point, const double bucketSize, const int64_t gridSizeX)
{
    int64_t x = std::max<int64_t>(static_cast<int64_t>(Utils::FastFloor<double>(point.x) / bucketSize), 0);
    int64_t y = std::max<int64_t>(static_cast<int64_t>(Utils::FastFloor<double>(point.y) / bucketSize), 0);

    return (y * gridSizeX) + x;
}

void HashGrid::Set(const Terra::Vector2& point, int64_t index)
{
    int64_t hash = HashPos(point, this->bucketSize, this->gridSizeX);
    if(this->hashtable[hash] > -1)
    {
        throw double_index_error();
    }

    this->hashtable[hash] = index;
}

int64_t HashGrid::At(const Terra::Vector2& point)
{
    int64_t hash = HashPos(point, this->bucketSize, this->gridSizeX);
    return this->hashtable[hash];
}

std::vector<int64_t> HashGrid::Neighbours(const Terra::Vector2& point)
{
    std::vector<int64_t> indexs;

    int64_t x = static_cast<int64_t>(Utils::FastFloor<double>(point.x) / this->bucketSize);
    int64_t y = static_cast<int64_t>(Utils::FastFloor<double>(point.y) / this->bucketSize);

    const int64_t x0 = std::max<int64_t>(x - neighbours, 0);
    const int64_t y0 = std::max<int64_t>(y - neighbours, 0);
    const int64_t x1 = std::min<int64_t>(x + neighbours + 1, this->gridSizeX);
    const int64_t y1 = std::min<int64_t>(y + neighbours + 1, this->gridSizeY);

    for (y = y0; y < y1; y++)
    {
        for (x = x0; x < x1; x++)
        {
            int64_t index = hashtable[(y * this->gridSizeX) + x];
            if (index > -1)
            {
                indexs.push_back(index);
            }
        }
    }

    return indexs;
}
