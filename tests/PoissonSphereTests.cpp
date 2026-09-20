#include "PoissonSphere.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

int main()
{
    constexpr std::uint64_t count{500000001};
    constexpr std::size_t sampleCount{16};
    const World::PoissonSphere sphere{count, 42};
    std::array<World::Point3, sampleCount> points{};
    sphere.Generate(count - sampleCount, points);
    for (const auto& point : points)
    {
        const double length{std::sqrt(point.X * point.X + point.Y * point.Y + point.Z * point.Z)};
        if (std::abs(length - 1.0) > 1.0e-12)
        {
            std::cerr << "Generated point is not on the unit sphere\n";
            return 1;
        }
    }
    return 0;
}
