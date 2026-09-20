#include "ColorMap.h"

#include "PngImage.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <utility>

namespace World
{
    ColorMap::ColorMap(std::uint32_t width, std::uint32_t height,
        std::uint16_t maxValue, std::vector<Color16> pixels)
        : m_Width{width}, m_Height{height}, m_MaxValue{maxValue}, m_Pixels{std::move(pixels)}
    {
    }

    ColorMap ColorMap::Load(const std::filesystem::path& path)
    {
        const auto image{LoadPngImage(path)};
        const auto pixelCount{static_cast<std::size_t>(image.Width) * image.Height};
        std::vector<Color16> pixels{};
        pixels.reserve(pixelCount);
        for (std::size_t index = 0; index < pixelCount; ++index)
        {
            const std::size_t offset{index * image.ChannelCount};
            const auto red{image.Samples[offset]};
            const auto green{image.ColorType == 2 || image.ColorType == 6
                ? image.Samples[offset + 1] : red};
            const auto blue{image.ColorType == 2 || image.ColorType == 6
                ? image.Samples[offset + 2] : red};
            if ((image.ColorType == 4 && image.Samples[offset + 1] != image.MaxValue) ||
                (image.ColorType == 6 && image.Samples[offset + 3] != image.MaxValue))
                throw std::runtime_error{"Color-map alpha channel must be fully opaque"};
            pixels.emplace_back(Color16{red, green, blue});
        }
        return {image.Width, image.Height, image.MaxValue, std::move(pixels)};
    }

    Color3 ColorMap::Sample(const Point3& direction) const
    {
        const double longitude{std::atan2(direction.Y, direction.X)};
        const double latitude{std::asin(std::clamp(direction.Z, -1.0, 1.0))};
        const double x{(longitude / (2.0 * std::numbers::pi) + 0.5) * m_Width - 0.5};
        const double y{(0.5 - latitude / std::numbers::pi) * m_Height - 0.5};
        const auto left{static_cast<std::int64_t>(std::floor(x))};
        const auto top{static_cast<std::int64_t>(std::floor(y))};
        const double tx{x - std::floor(x)};
        const double ty{y - std::floor(y)};
        const auto wrap{[this](std::int64_t index) {
            const auto width{static_cast<std::int64_t>(m_Width)};
            return static_cast<std::size_t>((index % width + width) % width);
        }};
        const auto clamp{[this](std::int64_t index) {
            return static_cast<std::size_t>(std::clamp(index, std::int64_t{0},
                static_cast<std::int64_t>(m_Height) - 1));
        }};
        const auto pixel{[this, &wrap, &clamp](std::int64_t column, std::int64_t row) {
            return m_Pixels[clamp(row) * m_Width + wrap(column)];
        }};
        const auto upperLeft{pixel(left, top)};
        const auto upperRight{pixel(left + 1, top)};
        const auto lowerLeft{pixel(left, top + 1)};
        const auto lowerRight{pixel(left + 1, top + 1)};
        const auto interpolate{[tx, ty](std::uint16_t upperLeftValue,
            std::uint16_t upperRightValue, std::uint16_t lowerLeftValue,
            std::uint16_t lowerRightValue) {
            const double upper{std::lerp(static_cast<double>(upperLeftValue),
                static_cast<double>(upperRightValue), tx)};
            const double lower{std::lerp(static_cast<double>(lowerLeftValue),
                static_cast<double>(lowerRightValue), tx)};
            return std::lerp(upper, lower, ty);
        }};
        const double scale{255.0 / m_MaxValue};
        const auto toByte{[scale](double value) {
            return static_cast<std::uint8_t>(std::clamp(std::lround(value * scale), 0l, 255l));
        }};
        return {toByte(interpolate(upperLeft.Red, upperRight.Red, lowerLeft.Red, lowerRight.Red)),
            toByte(interpolate(upperLeft.Green, upperRight.Green, lowerLeft.Green, lowerRight.Green)),
            toByte(interpolate(upperLeft.Blue, upperRight.Blue, lowerLeft.Blue, lowerRight.Blue))};
    }
} // namespace World
