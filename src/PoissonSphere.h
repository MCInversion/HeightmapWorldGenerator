#pragma once

#include "Geometry.h"

#include <array>
#include <cstdint>
#include <span>

namespace World
{
    /**
     * \class PoissonSphere
     * \brief Indexed minimum-distance sampling of the unit sphere.
     *
     * The generator uses a spherical Fibonacci sequence, whose uniform area and
     * global separation provide a scalable Poisson-disk-like distribution. It
     * can generate arbitrary ranges independently, so output size is not tied
     * to available memory. The seed applies a uniform deterministic 3D rotation.
     */
    class PoissonSphere final
    {
    public:
        PoissonSphere(std::uint64_t count, std::uint64_t seed);

        /// \brief Fill output with samples beginning at the requested global index.
        void Generate(std::uint64_t first, std::span<Point3> output) const;

        [[nodiscard]] std::uint64_t Count() const noexcept;

    private:
        std::uint64_t m_Count{};
        std::array<double, 9> m_Rotation{};
    };
} // namespace World
