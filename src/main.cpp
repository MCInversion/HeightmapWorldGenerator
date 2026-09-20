#include "BinaryPly.h"
#include "ColorMap.h"
#include "Heightmap.h"
#include "LasPointWriter.h"
#include "PointWriter.h"
#include "PoissonSphere.h"
#include "SurfaceProjector.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace World
{
    namespace
    {
        enum class OutputFormat
        {
            Ply,
            Las,
            Laz
        };

        struct Options
        {
            std::filesystem::path Input{};
            std::filesystem::path Output{};
            std::filesystem::path ColorMap{};
            double Radius{};      ///< Metres.
            double Density{};     ///< Points per square kilometre.
            double HeightMin{};   ///< Metres above datum at black.
            double HeightMax{};   ///< Metres above datum at white.
            double HeightScale{1.0};
            std::uint64_t PointCount{};
            std::uint64_t Seed{42};
            bool Normals{};
        };

        [[nodiscard]] double ParseDouble(const std::string& text)
        {
            std::size_t used{};
            const double value{std::stod(text, &used)};
            if (used != text.size() || !std::isfinite(value))
                throw std::runtime_error{"Expected finite number: " + text};
            return value;
        }

        [[nodiscard]] std::uint64_t ParseUnsigned(const std::string& text)
        {
            if (text.empty() || text.front() == '-')
                throw std::runtime_error{"Expected unsigned integer: " + text};
            std::size_t used{};
            const auto value{std::stoull(text, &used)};
            if (used != text.size())
                throw std::runtime_error{"Expected unsigned integer: " + text};
            return value;
        }

        [[nodiscard]] Options ParseOptions(int argc, char** argv)
        {
            Options options{};
            for (int i = 1; i < argc; ++i)
            {
                const std::string key{argv[i]};
                if (key == "--help")
                {
                    std::cout << "Usage: HeightmapWorldGenerator --input map.png --output world.{ply|las|laz} "
                        "--radius METRES (--density POINTS_PER_KM2 | --point-count COUNT) "
                        "--height-min METRES --height-max METRES [--height-scale FACTOR] "
                        "[--color-map texture.png] [--seed INTEGER] [--normals]\n";
                    std::exit(0);
                }
                if (key == "--normals")
                {
                    options.Normals = true;
                    continue;
                }
                if (i + 1 >= argc)
                    throw std::runtime_error{"Missing value for " + key};
                const std::string value{argv[++i]};
                if (key == "--input") options.Input = value;
                else if (key == "--output") options.Output = value;
                else if (key == "--color-map") options.ColorMap = value;
                else if (key == "--radius") options.Radius = ParseDouble(value);
                else if (key == "--density") options.Density = ParseDouble(value);
                else if (key == "--height-min") options.HeightMin = ParseDouble(value);
                else if (key == "--height-max") options.HeightMax = ParseDouble(value);
                else if (key == "--height-scale") options.HeightScale = ParseDouble(value);
                else if (key == "--point-count") options.PointCount = ParseUnsigned(value);
                else if (key == "--seed") options.Seed = ParseUnsigned(value);
                else throw std::runtime_error{"Unknown option: " + key};
            }
            if (options.Input.empty() || options.Output.empty() || options.Radius <= 0.0 ||
                options.HeightMax < options.HeightMin || options.HeightScale <= 0.0 ||
                (options.Density <= 0.0) == (options.PointCount == 0) ||
                options.Radius + options.HeightMin * options.HeightScale <= 0.0)
                throw std::runtime_error{"Invalid or missing options; use --help"};
            return options;
        }

        [[nodiscard]] OutputFormat GetOutputFormat(const std::filesystem::path& output)
        {
            std::string extension{output.extension().string()};
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character) {
                return static_cast<char>(std::tolower(character));
            });
            if (extension == ".ply") return OutputFormat::Ply;
            if (extension == ".las") return OutputFormat::Las;
            if (extension == ".laz") return OutputFormat::Laz;
            throw std::runtime_error{"Output extension must be .ply, .las, or .laz"};
        }

        [[nodiscard]] std::uint64_t CalculatePointCount(const Options& options)
        {
            constexpr std::uint64_t maxOutputPoints{1ull << 53u};
            if (options.PointCount != 0)
            {
                if (options.PointCount > maxOutputPoints)
                    throw std::runtime_error{"Point count exceeds the indexed sampler's precision limit"};
                return options.PointCount;
            }

            const long double radiusKm{static_cast<long double>(options.Radius) / 1000.0L};
            const long double wanted{4.0L * std::numbers::pi_v<long double> * radiusKm * radiusKm
                * static_cast<long double>(options.Density)};
            if (!std::isfinite(wanted) || wanted < 0.5L || wanted > maxOutputPoints)
                throw std::runtime_error{"Density yields zero points or exceeds the indexed sampler's precision limit"};
            return static_cast<std::uint64_t>(std::round(wanted));
        }
    } // namespace
} // namespace World

int main(int argc, char** argv)
{
    try
    {
        const auto options{World::ParseOptions(argc, argv)};
        const auto outputFormat{World::GetOutputFormat(options.Output)};
        const std::uint64_t count{World::CalculatePointCount(options)};
        const auto heightmap{World::Heightmap::Load(options.Input)};
        std::optional<World::ColorMap> colorMap{};
        if (!options.ColorMap.empty())
            colorMap.emplace(World::ColorMap::Load(options.ColorMap));
        const World::PoissonSphere sampler{count, options.Seed};
        const World::SurfaceProjector projector{heightmap, options.Radius,
            options.HeightMin, options.HeightMax, options.HeightScale};
        std::unique_ptr<World::PointWriter> writer{};
        if (outputFormat == World::OutputFormat::Ply)
        {
            writer = std::make_unique<World::BinaryPlyWriter>(options.Output, count,
                options.Normals, colorMap.has_value());
        }
        else
        {
            const double maximumCoordinate{options.Radius + options.HeightMax * options.HeightScale};
            writer = std::make_unique<World::LasPointWriter>(options.Output, count,
                options.Normals, colorMap.has_value(), outputFormat == World::OutputFormat::Laz,
                maximumCoordinate);
        }
        const std::size_t chunkPoints{options.Normals ? 1u << 19u : 1u << 20u};
        std::vector<World::Point3> points{};
        points.reserve(chunkPoints);
        std::vector<World::Point3> normals{};
        if (options.Normals)
            normals.reserve(chunkPoints);
        std::vector<World::Color3> colors{};
        if (colorMap.has_value())
            colors.reserve(chunkPoints);
        const unsigned workers{std::max(1u, std::thread::hardware_concurrency())};
        const auto project{[&points, &normals, &colors, &projector, &colorMap,
            &options](std::size_t first, std::size_t last) {
            for (std::size_t i = first; i < last; ++i)
            {
                const auto direction{points[i]};
                if (colorMap.has_value())
                    colors[i] = colorMap->Sample(direction);
                projector.Project(direction, points[i], options.Normals ? &normals[i] : nullptr);
            }
        }};
        const std::size_t bytesPerVertex{outputFormat == World::OutputFormat::Ply
            ? sizeof(World::Point3) * (options.Normals ? 2u : 1u)
                + (colorMap.has_value() ? sizeof(World::Color3) : 0u)
            : (colorMap.has_value() ? 36u : 30u) + (options.Normals ? 12u : 0u)};
        const long double gibibytes{static_cast<long double>(count) * bytesPerVertex
            / (1024.0L * 1024.0L * 1024.0L)};
        std::cout << "Generating " << count << " vertices (approximately "
                  << static_cast<double>(gibibytes) << " GiB"
                  << (outputFormat == World::OutputFormat::Laz ? " before LAZ compression" : "")
                  << ")...\n";

        for (std::uint64_t processed = 0; processed < count;)
        {
            const auto remaining{count - processed};
            const auto current{static_cast<std::size_t>(std::min<std::uint64_t>(remaining, chunkPoints))};
            points.resize(current);
            if (options.Normals)
                normals.resize(current);
            if (colorMap.has_value())
                colors.resize(current);
            sampler.Generate(processed, points);
            const std::size_t workerCount{std::min(static_cast<std::size_t>(workers),
                std::max(std::size_t{1}, current / 10000))};
            std::vector<std::thread> threads{};
            threads.reserve(workerCount - 1);
            for (std::size_t worker = 1; worker < workerCount; ++worker)
            {
                const std::size_t first{current * worker / workerCount};
                const std::size_t last{current * (worker + 1) / workerCount};
                threads.emplace_back(project, first, last);
            }
            project(0, current / workerCount);
            for (auto& thread : threads)
                thread.join();
            writer->Write(points, normals, colors);
            processed += current;
            std::cout << '\r' << processed << " / " << count << std::flush;
        }
        writer->Finish();
        std::cout << "\nWrote " << count << " vertices to " << options.Output.string() << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
