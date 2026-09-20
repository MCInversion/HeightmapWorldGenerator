#pragma once

#include "PointWriter.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <vector>

namespace World
{
    /**
     * \class BinaryPlyWriter
     * \brief Streaming binary little-endian PLY vertex writer.
     *
     * Exactly vertexCount points must be supplied before Finish. Data is written
     * incrementally, allowing files much larger than memory and larger than 4 GiB.
     */
    class BinaryPlyWriter final : public PointWriter
    {
    public:
        BinaryPlyWriter(const std::filesystem::path& path,
            std::uint64_t vertexCount,
            bool includeNormals,
            bool includeColors);
        ~BinaryPlyWriter() override;

        BinaryPlyWriter(const BinaryPlyWriter&) = delete;
        BinaryPlyWriter& operator=(const BinaryPlyWriter&) = delete;

        void Write(std::span<const Point3> points,
            std::span<const Point3> normals = {},
            std::span<const Color3> colors = {}) override;
        void Finish() override;

    private:
        std::ofstream m_Output{};
        std::uint64_t m_ExpectedCount{};
        std::uint64_t m_WrittenCount{};
        bool m_IncludeNormals{};
        bool m_IncludeColors{};
        bool m_Finished{};
        std::vector<char> m_Buffer{};
    };
} // namespace World
