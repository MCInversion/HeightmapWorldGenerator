#pragma once

#include "Geometry.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace World
{
    /**
     * \brief Equirectangular grayscale map sampled at pixel centres.
     * Left/right wrap at the longitude seam; latitude is clamped at the poles.
     * Stored values are normalized to [0,1] without losing 16-bit input precision.
     * PNG RGB channels must be equal and alpha, when present, must be opaque.
     */
    class Heightmap final
    {
    public:
        [[nodiscard]] static Heightmap Load(const std::filesystem::path& path);
        [[nodiscard]] double Sample(const Point3& direction) const;
        /// \brief Recommended angular half-width in radians for derivative sampling.
        [[nodiscard]] double NormalSampleStep() const noexcept;

    private:
        std::uint32_t m_Width{};
        std::uint32_t m_Height{};
        std::vector<std::uint16_t> m_Pixels{};
        std::uint16_t m_MaxValue{};

        Heightmap(std::uint32_t width, std::uint32_t height,
            std::vector<std::uint16_t> pixels, std::uint16_t maxValue);
        [[nodiscard]] static Heightmap LoadPng(const std::filesystem::path& path);
        [[nodiscard]] static Heightmap LoadPgm(const std::filesystem::path& path);
    };
} // namespace World
