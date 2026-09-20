#include "LasPointWriter.h"

#include <laszip_api.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace World
{
    namespace
    {
        [[nodiscard]] std::string ErrorMessage(laszip_POINTER writer, const std::string& action)
        {
            laszip_CHAR* detail{};
            if (writer != nullptr && laszip_get_error(writer, &detail) == 0 && detail != nullptr && detail[0] != '\0')
                return action + ": " + detail;
            return action;
        }

        void Check(laszip_POINTER writer, laszip_I32 result, const std::string& action)
        {
            if (result != 0)
                throw std::runtime_error{ErrorMessage(writer, action)};
        }

        [[nodiscard]] double CoordinateScale(double maximumCoordinate)
        {
            constexpr double safety{0.95};
            const double required{maximumCoordinate /
                (static_cast<double>(std::numeric_limits<laszip_I32>::max()) * safety)};
            double scale{0.001};
            while (scale < required)
                scale *= 10.0;
            return scale;
        }

        void StoreFloat(laszip_U8* destination, double value) noexcept
        {
            const float narrowed{static_cast<float>(value)};
            std::memcpy(destination, &narrowed, sizeof(narrowed));
        }
    } // namespace

    struct LasPointWriter::Implementation
    {
        laszip_POINTER Writer{};
        laszip_point_struct* Point{};
        std::uint64_t ExpectedCount{};
        std::uint64_t WrittenCount{};
        bool IncludeNormals{};
        bool IncludeColors{};
        bool Finished{};
        bool Open{};

        ~Implementation()
        {
            if (Writer == nullptr)
                return;
            if (Open)
                (void)laszip_close_writer(Writer);
            (void)laszip_destroy(Writer);
        }
    };

    LasPointWriter::LasPointWriter(const std::filesystem::path& path,
        std::uint64_t vertexCount,
        bool includeNormals,
        bool includeColors,
        bool compressed,
        double maximumCoordinate)
        : m_Impl{std::make_unique<Implementation>()}
    {
        if (!std::isfinite(maximumCoordinate) || maximumCoordinate <= 0.0)
            throw std::runtime_error{"Invalid LAS coordinate bound"};

        Check(nullptr, laszip_create(&m_Impl->Writer), "Cannot create LASzip writer");
        m_Impl->ExpectedCount = vertexCount;
        m_Impl->IncludeNormals = includeNormals;
        m_Impl->IncludeColors = includeColors;

        laszip_header_struct* header{};
        Check(m_Impl->Writer, laszip_get_header_pointer(m_Impl->Writer, &header),
            "Cannot access LAS header");

        header->version_major = 1;
        header->version_minor = 4;
        header->global_encoding = 1u << 4u;
        std::strncpy(header->system_identifier, "HeightmapWorldGenerator", sizeof(header->system_identifier));
        std::strncpy(header->generating_software, "HeightmapWorldGenerator 1.0", sizeof(header->generating_software));
        header->header_size = 375;
        header->offset_to_point_data = 375;
        header->point_data_format = includeColors ? 7 : 6;
        header->point_data_record_length = static_cast<laszip_U16>((includeColors ? 36u : 30u)
            + (includeNormals ? 12u : 0u));
        header->number_of_point_records = 0;
        header->extended_number_of_point_records = vertexCount;
        header->extended_number_of_points_by_return[0] = vertexCount;

        const double coordinateScale{CoordinateScale(maximumCoordinate)};
        header->x_scale_factor = coordinateScale;
        header->y_scale_factor = coordinateScale;
        header->z_scale_factor = coordinateScale;
        header->x_offset = 0.0;
        header->y_offset = 0.0;
        header->z_offset = 0.0;
        header->min_x = -maximumCoordinate;
        header->max_x = maximumCoordinate;
        header->min_y = -maximumCoordinate;
        header->max_y = maximumCoordinate;
        header->min_z = -maximumCoordinate;
        header->max_z = maximumCoordinate;

        if (includeNormals)
        {
            Check(m_Impl->Writer, laszip_add_attribute(m_Impl->Writer, 8, "normal_x",
                "Unit surface normal X", 1.0, 0.0), "Cannot add LAS normal_x attribute");
            Check(m_Impl->Writer, laszip_add_attribute(m_Impl->Writer, 8, "normal_y",
                "Unit surface normal Y", 1.0, 0.0), "Cannot add LAS normal_y attribute");
            Check(m_Impl->Writer, laszip_add_attribute(m_Impl->Writer, 8, "normal_z",
                "Unit surface normal Z", 1.0, 0.0), "Cannot add LAS normal_z attribute");
        }
        if (compressed)
        {
            Check(m_Impl->Writer, laszip_request_native_extension(m_Impl->Writer, 1),
                "Cannot enable native LAS 1.4 LAZ compression");
        }

#ifdef _WIN32
        const auto utf8Path{path.u8string()};
        const std::string filename{reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size()};
#else
        const std::string filename{path.string()};
#endif
        Check(m_Impl->Writer, laszip_open_writer(m_Impl->Writer, filename.c_str(), compressed ? 1 : 0),
            "Cannot create output " + std::string{compressed ? "LAZ" : "LAS"});
        m_Impl->Open = true;
        Check(m_Impl->Writer, laszip_get_point_pointer(m_Impl->Writer, &m_Impl->Point),
            "Cannot access LAS point buffer");
        m_Impl->Point->extended_point_type = 1;
        m_Impl->Point->extended_return_number = 1;
        m_Impl->Point->extended_number_of_returns = 1;
    }

    LasPointWriter::~LasPointWriter() = default;

    void LasPointWriter::Write(std::span<const Point3> points,
        std::span<const Point3> normals,
        std::span<const Color3> colors)
    {
        auto& implementation{*m_Impl};
        if (implementation.Finished || points.size() > implementation.ExpectedCount - implementation.WrittenCount)
            throw std::runtime_error{"Too many LAS vertices supplied"};
        if ((implementation.IncludeNormals && normals.size() != points.size()) ||
            (!implementation.IncludeNormals && !normals.empty()))
            throw std::runtime_error{"LAS normal data does not match its header"};
        if ((implementation.IncludeColors && colors.size() != points.size()) ||
            (!implementation.IncludeColors && !colors.empty()))
            throw std::runtime_error{"LAS color data does not match its header"};

        for (std::size_t index = 0; index < points.size(); ++index)
        {
            laszip_F64 coordinates[3]{points[index].X, points[index].Y, points[index].Z};
            Check(implementation.Writer, laszip_set_coordinates(implementation.Writer, coordinates),
                "LAS coordinate cannot be quantized");
            if (implementation.IncludeColors)
            {
                implementation.Point->rgb[0] = static_cast<laszip_U16>(colors[index].Red) * 257u;
                implementation.Point->rgb[1] = static_cast<laszip_U16>(colors[index].Green) * 257u;
                implementation.Point->rgb[2] = static_cast<laszip_U16>(colors[index].Blue) * 257u;
            }
            if (implementation.IncludeNormals)
            {
                StoreFloat(implementation.Point->extra_bytes, normals[index].X);
                StoreFloat(implementation.Point->extra_bytes + 4, normals[index].Y);
                StoreFloat(implementation.Point->extra_bytes + 8, normals[index].Z);
            }
            Check(implementation.Writer, laszip_update_inventory(implementation.Writer),
                "Cannot update LAS inventory");
            Check(implementation.Writer, laszip_write_point(implementation.Writer),
                "Cannot write LAS point");
        }
        implementation.WrittenCount += static_cast<std::uint64_t>(points.size());
    }

    void LasPointWriter::Finish()
    {
        auto& implementation{*m_Impl};
        if (implementation.WrittenCount != implementation.ExpectedCount)
            throw std::runtime_error{"LAS vertex count does not match its header"};
        Check(implementation.Writer, laszip_close_writer(implementation.Writer),
            "Cannot finalize LAS output");
        implementation.Open = false;
        implementation.Finished = true;
    }
} // namespace World
