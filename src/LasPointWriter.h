#pragma once

#include "PointWriter.h"

#include <cstdint>
#include <filesystem>
#include <memory>

namespace World
{
    /**
     * \class LasPointWriter
     * \brief Streaming LAS 1.4 / LAZ point writer backed by LASzip.
     */
    class LasPointWriter final : public PointWriter
    {
    public:
        LasPointWriter(const std::filesystem::path& path,
            std::uint64_t vertexCount,
            bool includeNormals,
            bool includeColors,
            bool compressed,
            double maximumCoordinate);
        ~LasPointWriter() override;

        LasPointWriter(const LasPointWriter&) = delete;
        LasPointWriter& operator=(const LasPointWriter&) = delete;

        void Write(std::span<const Point3> points,
            std::span<const Point3> normals = {},
            std::span<const Color3> colors = {}) override;
        void Finish() override;

    private:
        struct Implementation;
        std::unique_ptr<Implementation> m_Impl;
    };
} // namespace World
