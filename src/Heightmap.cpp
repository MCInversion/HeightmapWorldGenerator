#include "Heightmap.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>
#include <utility>

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
                throw std::runtime_error{"Input exceeds 1 GiB safety limit"};
            std::ifstream input{path, std::ios::binary};
            if (!input)
                throw std::runtime_error{"Cannot open heightmap"};
            return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
        }

        [[nodiscard]] std::string PgmToken(std::istream& input)
        {
            for (int character = input.get(); character != EOF; character = input.get())
            {
                if (character == '#')
                {
                    input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    continue;
                }
                if (std::isspace(static_cast<unsigned char>(character)) != 0)
                    continue;
                std::string token{static_cast<char>(character)};
                for (character = input.get(); character != EOF; character = input.get())
                {
                    if (std::isspace(static_cast<unsigned char>(character)) != 0)
                    {
                        if (character == '\r' && input.peek() == '\n')
                            input.get();
                        return token;
                    }
                    if (character == '#')
                    {
                        input.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                        return token;
                    }
                    token += static_cast<char>(character);
                    if (token.size() > 32)
                        throw std::runtime_error{"Invalid PGM token"};
                }
                return token;
            }
            throw std::runtime_error{"Unexpected end of PGM"};
        }

        [[nodiscard]] std::uint32_t ParseInteger(const std::string& token)
        {
            std::size_t used{};
            const auto number{std::stoull(token, &used)};
            if (used != token.size() || number > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error{"Invalid PGM integer"};
            return static_cast<std::uint32_t>(number);
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

    Heightmap::Heightmap(std::uint32_t width, std::uint32_t height,
        std::vector<std::uint16_t> pixels, std::uint16_t maxValue)
        : m_Width{width}, m_Height{height}, m_Pixels{std::move(pixels)}, m_MaxValue{maxValue}
    {
    }

    Heightmap Heightmap::Load(const std::filesystem::path& path)
    {
        auto extension{path.extension().string()};
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (extension == ".png")
            return LoadPng(path);
        if (extension == ".pgm")
            return LoadPgm(path);
        throw std::runtime_error{"Supported heightmap formats: .png (grayscale 8/16-bit), .pgm (P2/P5)"};
    }

    Heightmap Heightmap::LoadPgm(const std::filesystem::path& path)
    {
        std::ifstream input{path, std::ios::binary};
        if (!input)
            throw std::runtime_error{"Cannot open PGM"};
        const std::string magic{PgmToken(input)};
        if (magic != "P2" && magic != "P5")
            throw std::runtime_error{"Expected P2 or P5 PGM"};
        const auto width{ParseInteger(PgmToken(input))};
        const auto height{ParseInteger(PgmToken(input))};
        const auto maximum{ParseInteger(PgmToken(input))};
        const auto count{static_cast<std::uint64_t>(width) * height};
        if (count == 0 || count > MAX_PIXELS || maximum == 0 || maximum > 65535)
            throw std::runtime_error{"Invalid PGM dimensions or maximum value"};
        std::vector<std::uint16_t> pixels(static_cast<std::size_t>(count));
        if (magic == "P2")
        {
            for (auto& pixel : pixels)
            {
                const auto value{ParseInteger(PgmToken(input))};
                if (value > maximum)
                    throw std::runtime_error{"PGM pixel exceeds declared maximum"};
                pixel = static_cast<std::uint16_t>(value);
            }
        }
        else
        {
            for (auto& pixel : pixels)
            {
                const int high{input.get()};
                const int low{maximum > 255 ? input.get() : 0};
                if (high == EOF || low == EOF)
                    throw std::runtime_error{"Truncated PGM pixels"};
                const auto value{static_cast<std::uint32_t>(maximum > 255 ? high * 256 + low : high)};
                if (value > maximum)
                    throw std::runtime_error{"PGM pixel exceeds declared maximum"};
                pixel = static_cast<std::uint16_t>(value);
            }
        }
        return {width, height, std::move(pixels), static_cast<std::uint16_t>(maximum)};
    }

    Heightmap Heightmap::LoadPng(const std::filesystem::path& path)
    {
        const auto file{ReadFile(path)};
        constexpr std::array<unsigned char, 8> signature{137, 80, 78, 71, 13, 10, 26, 10};
        if (file.size() < signature.size() || !std::equal(signature.begin(), signature.end(), file.begin()))
            throw std::runtime_error{"Invalid PNG signature"};

        std::size_t position{signature.size()};
        std::uint32_t width{};
        std::uint32_t height{};
        unsigned bitDepth{};
        unsigned colorType{};
        unsigned channelCount{};
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
                width = ReadBigEndian(payload);
                height = ReadBigEndian(payload + 4);
                bitDepth = payload[8];
                colorType = payload[9];
                if ((bitDepth != 8 && bitDepth != 16) ||
                    (colorType != 0 && colorType != 2 && colorType != 4 && colorType != 6) ||
                    payload[10] != 0 || payload[11] != 0 || payload[12] != 0)
                    throw std::runtime_error{"PNG must be non-interlaced 8/16-bit gray, gray-alpha, RGB, or RGBA"};
                channelCount = colorType == 0 ? 1u : colorType == 2 ? 3u : colorType == 4 ? 2u : 4u;
                if (width == 0 || height == 0 || static_cast<std::uint64_t>(width) * height > MAX_PIXELS)
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

        const std::size_t bytesPerSample{bitDepth / 8u};
        const std::size_t bytesPerPixel{bytesPerSample * channelCount};
        const std::uint64_t rowBytes{static_cast<std::uint64_t>(width) * bytesPerPixel};
        const std::uint64_t decodedBytes{(rowBytes + 1u) * height};
        if (decodedBytes > MAX_INPUT_BYTES || compressed.size() > std::numeric_limits<uLong>::max())
            throw std::runtime_error{"PNG raster exceeds 1 GiB safety limit"};
        std::vector<unsigned char> filtered(static_cast<std::size_t>(decodedBytes));
        uLongf actual{static_cast<uLongf>(filtered.size())};
        if (uncompress(filtered.data(), &actual, compressed.data(),
            static_cast<uLong>(compressed.size())) != Z_OK || actual != filtered.size())
            throw std::runtime_error{"PNG decompression failed or raster length is wrong"};

        std::vector<std::uint16_t> pixels(static_cast<std::size_t>(width) * height);
        std::vector<unsigned char> previous(static_cast<std::size_t>(rowBytes), 0);
        std::vector<unsigned char> row(static_cast<std::size_t>(rowBytes));
        for (std::size_t y = 0; y < height; ++y)
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
            for (std::size_t x = 0; x < width; ++x)
            {
                const std::size_t offset{x * bytesPerPixel};
                const auto sample{[&row, bytesPerSample, offset](std::size_t channel) {
                    const std::size_t samplePosition{offset + channel * bytesPerSample};
                    return static_cast<std::uint16_t>(bytesPerSample == 1 ? row[samplePosition]
                        : (static_cast<unsigned>(row[samplePosition]) << 8u) | row[samplePosition + 1]);
                }};
                const auto gray{sample(0)};
                if ((colorType == 2 || colorType == 6) && (sample(1) != gray || sample(2) != gray))
                    throw std::runtime_error{"RGB PNG heightmap must have equal red, green, and blue channels"};
                const auto maximum{static_cast<std::uint16_t>(bitDepth == 8 ? 255 : 65535)};
                if ((colorType == 4 && sample(1) != maximum) ||
                    (colorType == 6 && sample(3) != maximum))
                    throw std::runtime_error{"PNG heightmap alpha channel must be fully opaque"};
                pixels[y * width + x] = gray;
            }
            previous.swap(row);
        }
        return {width, height, std::move(pixels), static_cast<std::uint16_t>(bitDepth == 8 ? 255 : 65535)};
    }

    double Heightmap::Sample(const Point3& direction) const
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
        const auto pixel{[this, &wrap, &clamp](std::int64_t column, std::int64_t line) {
            return static_cast<double>(m_Pixels[clamp(line) * m_Width + wrap(column)]);
        }};
        const double upper{std::lerp(pixel(left, top), pixel(left + 1, top), tx)};
        const double lower{std::lerp(pixel(left, top + 1), pixel(left + 1, top + 1), tx)};
        return std::lerp(upper, lower, ty) / m_MaxValue;
    }

    double Heightmap::NormalSampleStep() const noexcept
    {
        const double longitudeStep{2.0 * std::numbers::pi / static_cast<double>(m_Width)};
        const double latitudeStep{std::numbers::pi / static_cast<double>(m_Height)};
        return 0.5 * std::min(longitudeStep, latitudeStep);
    }
} // namespace World
