#include "PngImage.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

#include <zlib.h>

namespace World
{
    namespace
    {
        constexpr std::uint64_t MAX_INPUT_BYTES{1024ull * 1024ull * 1024ull};
        constexpr std::uint64_t MAX_PIXELS{250000000ull};

        [[nodiscard]] std::uint32_t ReadBigEndian(const unsigned char* bytes)
        {
            return (static_cast<std::uint32_t>(bytes[0]) << 24u)
                 | (static_cast<std::uint32_t>(bytes[1]) << 16u)
                 | (static_cast<std::uint32_t>(bytes[2]) << 8u)
                 | static_cast<std::uint32_t>(bytes[3]);
        }

        [[nodiscard]] std::vector<unsigned char> ReadFile(const std::filesystem::path& path)
        {
            const auto size{std::filesystem::file_size(path)};
            if (size > MAX_INPUT_BYTES)
                throw std::runtime_error{"PNG input exceeds 1 GiB safety limit"};
            std::ifstream input{path, std::ios::binary};
            if (!input)
                throw std::runtime_error{"Cannot open PNG image"};
            return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        }

        [[nodiscard]] int Paeth(int a, int b, int c)
        {
            const int predictor{a + b - c};
            const int distanceA{std::abs(predictor - a)};
            const int distanceB{std::abs(predictor - b)};
            const int distanceC{std::abs(predictor - c)};
            if (distanceA <= distanceB && distanceA <= distanceC)
                return a;
            return distanceB <= distanceC ? b : c;
        }
    } // namespace

    PngImage LoadPngImage(const std::filesystem::path& path)
    {
        const auto file{ReadFile(path)};
        constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
        if (file.size() < signature.size() || !std::equal(signature.begin(), signature.end(), file.begin()))
            throw std::runtime_error{"Invalid PNG signature"};

        PngImage image{};
        std::size_t position{signature.size()};
        bool hasHeader{};
        bool hasEnd{};
        std::vector<unsigned char> compressed{};
        while (position + 12 <= file.size())
        {
            const auto length{ReadBigEndian(file.data() + position)};
            const auto remaining{file.size() - position - 12};
            if (length > remaining)
                throw std::runtime_error{"Truncated PNG chunk"};
            const auto* type{file.data() + position + 4};
            const auto* payload{type + 4};
            auto check{crc32(0L, Z_NULL, 0)};
            check = crc32(check, type, static_cast<uInt>(length + 4u));
            if (check != ReadBigEndian(payload + length))
                throw std::runtime_error{"PNG chunk CRC mismatch"};
            const std::string name{reinterpret_cast<const char*>(type), 4};
            if (name == "IHDR")
            {
                if (hasHeader || length != 13 || position != signature.size())
                    throw std::runtime_error{"Invalid PNG IHDR"};
                image.Width = ReadBigEndian(payload);
                image.Height = ReadBigEndian(payload + 4);
                image.BitDepth = payload[8];
                image.ColorType = payload[9];
                if ((image.BitDepth != 8 && image.BitDepth != 16) ||
                    (image.ColorType != 0 && image.ColorType != 2 &&
                        image.ColorType != 4 && image.ColorType != 6) ||
                    payload[10] != 0 || payload[11] != 0 || payload[12] != 0)
                    throw std::runtime_error{"PNG must be non-interlaced 8/16-bit gray, gray-alpha, RGB, or RGBA"};
                image.ChannelCount = image.ColorType == 0 ? 1u : image.ColorType == 2
                    ? 3u : image.ColorType == 4 ? 2u : 4u;
                image.MaxValue = static_cast<std::uint16_t>(image.BitDepth == 8 ? 255 : 65535);
                if (image.Width == 0 || image.Height == 0 ||
                    static_cast<std::uint64_t>(image.Width) * image.Height > MAX_PIXELS)
                    throw std::runtime_error{"Invalid PNG dimensions"};
                hasHeader = true;
            }
            else if (name == "IDAT")
            {
                if (!hasHeader || compressed.size() + static_cast<std::uint64_t>(length) > MAX_INPUT_BYTES)
                    throw std::runtime_error{"Invalid PNG image data"};
                compressed.insert(compressed.end(), payload, payload + length);
            }
            else if (name == "IEND")
            {
                if (length != 0)
                    throw std::runtime_error{"Invalid PNG end chunk"};
                hasEnd = true;
                break;
            }
            else if ((type[0] & 0x20u) == 0)
                throw std::runtime_error{"Unsupported critical PNG chunk"};
            position += 12u + length;
        }
        if (!hasHeader || !hasEnd || compressed.empty())
            throw std::runtime_error{"Incomplete PNG"};

        const std::size_t bytesPerSample{image.BitDepth / 8u};
        const std::size_t bytesPerPixel{bytesPerSample * image.ChannelCount};
        const std::uint64_t rowBytes{static_cast<std::uint64_t>(image.Width) * bytesPerPixel};
        const std::uint64_t decodedBytes{(rowBytes + 1u) * image.Height};
        if (decodedBytes > MAX_INPUT_BYTES || compressed.size() > std::numeric_limits<uLong>::max())
            throw std::runtime_error{"PNG raster exceeds 1 GiB safety limit"};
        std::vector<unsigned char> filtered(static_cast<std::size_t>(decodedBytes));
        uLongf actual{static_cast<uLongf>(filtered.size())};
        if (uncompress(filtered.data(), &actual, compressed.data(),
            static_cast<uLong>(compressed.size())) != Z_OK || actual != filtered.size())
            throw std::runtime_error{"PNG decompression failed or raster length is wrong"};

        const auto sampleCount{static_cast<std::size_t>(image.Width) * image.Height * image.ChannelCount};
        image.Samples.resize(sampleCount);
        std::vector<unsigned char> previous(static_cast<std::size_t>(rowBytes), 0);
        std::vector<unsigned char> row(static_cast<std::size_t>(rowBytes));
        for (std::size_t y = 0; y < image.Height; ++y)
        {
            const std::size_t start{y * static_cast<std::size_t>(rowBytes + 1u)};
            const unsigned filter{filtered[start]};
            if (filter > 4)
                throw std::runtime_error{"Unsupported PNG row filter"};
            for (std::size_t x = 0; x < row.size(); ++x)
            {
                const int a{x >= bytesPerPixel ? row[x - bytesPerPixel] : 0};
                const int b{previous[x]};
                const int c{x >= bytesPerPixel ? previous[x - bytesPerPixel] : 0};
                const int predictor{filter == 1 ? a : filter == 2 ? b : filter == 3
                    ? (a + b) / 2 : filter == 4 ? Paeth(a, b, c) : 0};
                row[x] = static_cast<unsigned char>((filtered[start + 1 + x] + predictor) & 255);
            }
            for (std::size_t x = 0; x < image.Width; ++x)
            {
                for (std::size_t channel = 0; channel < image.ChannelCount; ++channel)
                {
                    const std::size_t source{(x * image.ChannelCount + channel) * bytesPerSample};
                    image.Samples[(y * image.Width + x) * image.ChannelCount + channel]
                        = static_cast<std::uint16_t>(bytesPerSample == 1 ? row[source]
                            : (static_cast<unsigned>(row[source]) << 8u) | row[source + 1]);
                }
            }
            previous.swap(row);
        }
        return image;
    }
} // namespace World
