#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace World
{
    /**
     * \struct PngImage
     * \brief Decoded non-interlaced 8/16-bit PNG samples in source channel order.
     */
    struct PngImage
    {
        std::uint32_t Width{};
        std::uint32_t Height{};
        unsigned BitDepth{};
        unsigned ColorType{};
        unsigned ChannelCount{};
        std::uint16_t MaxValue{};
        std::vector<std::uint16_t> Samples{};
    };

    /// \brief Decode a gray, gray-alpha, RGB, or RGBA PNG and verify its checksums.
    [[nodiscard]] PngImage LoadPngImage(const std::filesystem::path& path);
} // namespace World
