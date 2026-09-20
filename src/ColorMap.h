#pragma once

#include "Geometry.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace World
{
    /**
     * \class ColorMap
     * \brief Bilinearly sampled equirectangular sRGB texture.
     *
     * Longitude wraps at the left/right seam and latitude clamps at the poles.
     * RGB, RGBA, grayscale, and grayscale-alpha 8/16-bit PNGs are supported;
     * alpha, when present, must be fully opaque.
     */
    class ColorMap final
    {
    public:
        [[nodiscard]] static ColorMap Load(const std::filesystem::path& path);
        [[nodiscard]] Color3 Sample(const Point3& direction) const;

    private:
        struct Color16
        {
            std::uint16_t Red{};
            std::uint16_t Green{};
            std::uint16_t Blue{};
        };

        std::uint32_t m_Width{};
        std::uint32_t m_Height{};
        std::uint16_t m_MaxValue{};
        std::vector<Color16> m_Pixels{};

        ColorMap(std::uint32_t width, std::uint32_t height,
            std::uint16_t maxValue, std::vector<Color16> pixels);
    };
} // namespace World
