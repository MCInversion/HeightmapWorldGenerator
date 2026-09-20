#pragma once

#include <cstdint>
#include <cmath>

namespace World
{
    /** \brief Three-component Cartesian point in unit sphere coordinates or metres. */
    struct Point3
    {
        double X{};
        double Y{};
        double Z{};
    };

    /// \brief Eight-bit sRGB point color for PLY output.
    struct Color3
    {
        std::uint8_t Red{};
        std::uint8_t Green{};
        std::uint8_t Blue{};
    };

    [[nodiscard]] inline double DistanceSquared(const Point3& a, const Point3& b)
    {
        const double dx{a.X - b.X};
        const double dy{a.Y - b.Y};
        const double dz{a.Z - b.Z};
        return dx * dx + dy * dy + dz * dz;
    }
} // namespace World
