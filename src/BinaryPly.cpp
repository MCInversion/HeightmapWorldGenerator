#include "BinaryPly.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace World
{
    namespace
    {
        void StoreDouble(char* destination, double value) noexcept
        {
            const auto bits{std::bit_cast<std::uint64_t>(value)};
            for (unsigned shift = 0; shift < 64; shift += 8)
                destination[shift / 8] = static_cast<char>((bits >> shift) & 0xffu);
        }

        void StorePoint(char* destination, const Point3& point) noexcept
        {
            StoreDouble(destination, point.X);
            StoreDouble(destination + sizeof(double), point.Y);
            StoreDouble(destination + sizeof(double) * 2, point.Z);
        }
    } // namespace

    BinaryPlyWriter::BinaryPlyWriter(const std::filesystem::path& path,
        std::uint64_t vertexCount,
        bool includeNormals,
        bool includeColors)
        : m_Output{path, std::ios::binary}, m_ExpectedCount{vertexCount},
          m_IncludeNormals{includeNormals}, m_IncludeColors{includeColors}
    {
        static_assert(sizeof(double) == 8);
        static_assert(sizeof(Point3) == sizeof(double) * 3);
        static_assert(sizeof(Color3) == 3);
        if (!m_Output)
            throw std::runtime_error{"Cannot create output PLY"};
        m_Output << "ply\nformat binary_little_endian 1.0\nelement vertex " << vertexCount
                 << "\nproperty double x\nproperty double y\nproperty double z\n";
        if (m_IncludeNormals)
            m_Output << "property double nx\nproperty double ny\nproperty double nz\n";
        if (m_IncludeColors)
            m_Output << "property uchar red\nproperty uchar green\nproperty uchar blue\n";
        m_Output << "end_header\n";
    }

    BinaryPlyWriter::~BinaryPlyWriter()
    {
        if (!m_Finished)
            m_Output.close();
    }

    void BinaryPlyWriter::Write(std::span<const Point3> points,
        std::span<const Point3> normals,
        std::span<const Color3> colors)
    {
        if (m_Finished || points.size() > m_ExpectedCount - m_WrittenCount)
            throw std::runtime_error{"Too many PLY vertices supplied"};
        if ((m_IncludeNormals && normals.size() != points.size()) ||
            (!m_IncludeNormals && !normals.empty()))
            throw std::runtime_error{"PLY normal data does not match its header"};
        if ((m_IncludeColors && colors.size() != points.size()) ||
            (!m_IncludeColors && !colors.empty()))
            throw std::runtime_error{"PLY color data does not match its header"};

        if constexpr (std::endian::native == std::endian::little)
        {
            if (!m_IncludeNormals && !m_IncludeColors)
            {
                const auto byteCount{points.size_bytes()};
                if (byteCount > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
                    throw std::runtime_error{"PLY write chunk exceeds stream limits"};
                m_Output.write(reinterpret_cast<const char*>(points.data()),
                    static_cast<std::streamsize>(byteCount));
            }
        }

        if (m_IncludeNormals || m_IncludeColors || std::endian::native != std::endian::little)
        {
            const std::size_t bytesPerVertex{sizeof(Point3)
                + (m_IncludeNormals ? sizeof(Point3) : 0u)
                + (m_IncludeColors ? sizeof(Color3) : 0u)};
            m_Buffer.resize(points.size() * bytesPerVertex);
            for (std::size_t index = 0; index < points.size(); ++index)
            {
                auto* destination{m_Buffer.data() + index * bytesPerVertex};
                StorePoint(destination, points[index]);
                destination += sizeof(Point3);
                if (m_IncludeNormals)
                {
                    StorePoint(destination, normals[index]);
                    destination += sizeof(Point3);
                }
                if (m_IncludeColors)
                {
                    destination[0] = static_cast<char>(colors[index].Red);
                    destination[1] = static_cast<char>(colors[index].Green);
                    destination[2] = static_cast<char>(colors[index].Blue);
                }
            }
            m_Output.write(m_Buffer.data(), static_cast<std::streamsize>(m_Buffer.size()));
        }
        if (!m_Output)
            throw std::runtime_error{"Failed writing output PLY"};
        m_WrittenCount += static_cast<std::uint64_t>(points.size());
    }

    void BinaryPlyWriter::Finish()
    {
        if (m_WrittenCount != m_ExpectedCount)
            throw std::runtime_error{"PLY vertex count does not match its header"};
        m_Output.close();
        if (!m_Output)
            throw std::runtime_error{"Failed finalizing output PLY"};
        m_Finished = true;
    }
} // namespace World
