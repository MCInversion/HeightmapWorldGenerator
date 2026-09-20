#include <laszip_api.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    [[nodiscard]] std::string Error(laszip_POINTER reader, const std::string& action)
    {
        laszip_CHAR* detail{};
        if (reader != nullptr && laszip_get_error(reader, &detail) == 0 && detail != nullptr)
            return action + ": " + detail;
        return action;
    }

    void Check(laszip_POINTER reader, laszip_I32 result, const std::string& action)
    {
        if (result != 0)
            throw std::runtime_error{Error(reader, action)};
    }
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 4)
            throw std::runtime_error{"Usage: LasOutputTests FILE EXPECTED_COMPRESSION SCHEMA"};
        const bool fullSchema{std::string{argv[3]} == "color-normals"};
        if (!fullSchema && std::string{argv[3]} != "position")
            throw std::runtime_error{"Unknown expected schema"};

        laszip_POINTER reader{};
        Check(nullptr, laszip_create(&reader), "Cannot create LASzip reader");
        struct DestroyReader
        {
            laszip_POINTER Value{};
            ~DestroyReader() { if (Value != nullptr) (void)laszip_destroy(Value); }
        } destroy{reader};

        laszip_BOOL compressed{};
        Check(reader, laszip_open_reader(reader, argv[1], &compressed), "Cannot open output");
        const bool expectedCompressed{std::string{argv[2]} == "compressed"};
        if ((compressed != 0) != expectedCompressed)
            throw std::runtime_error{"Unexpected compression state"};

        laszip_header_struct* header{};
        laszip_point_struct* point{};
        Check(reader, laszip_get_header_pointer(reader, &header), "Cannot read header");
        Check(reader, laszip_get_point_pointer(reader, &point), "Cannot access point" );
        if (header->version_major != 1 || header->version_minor != 4 ||
            header->point_data_format != (fullSchema ? 7 : 6) ||
            header->point_data_record_length != (fullSchema ? 48 : 30) ||
            header->number_of_point_records != 0 || header->extended_number_of_point_records != 1000)
            throw std::runtime_error{"Unexpected LAS header"};

        bool foundColor{};
        for (std::uint64_t index = 0; index < 1000; ++index)
        {
            Check(reader, laszip_read_point(reader), "Cannot read point");
            laszip_F64 coordinates[3]{};
            Check(reader, laszip_get_coordinates(reader, coordinates), "Cannot decode coordinates");
            const double radius{std::sqrt(coordinates[0] * coordinates[0]
                + coordinates[1] * coordinates[1] + coordinates[2] * coordinates[2])};
            if (!std::isfinite(radius) || radius < 999.0 || radius > 1501.0)
                throw std::runtime_error{"Invalid decoded coordinate"};
            if (fullSchema)
            {
                if (point->num_extra_bytes != 12 || point->extra_bytes == nullptr)
                    throw std::runtime_error{"Normal Extra Bytes are missing"};
                float normal[3]{};
                std::memcpy(normal, point->extra_bytes, sizeof(normal));
                const double normalLength{std::sqrt(static_cast<double>(normal[0]) * normal[0]
                    + static_cast<double>(normal[1]) * normal[1]
                    + static_cast<double>(normal[2]) * normal[2])};
                if (!std::isfinite(normalLength) || std::abs(normalLength - 1.0) > 0.001)
                    throw std::runtime_error{"Invalid normal Extra Bytes"};
                foundColor = foundColor || point->rgb[0] != 0 || point->rgb[1] != 0 || point->rgb[2] != 0;
            }
            else if (point->num_extra_bytes != 0)
            {
                throw std::runtime_error{"Unexpected Extra Bytes in position-only output"};
            }
        }
        if (fullSchema && !foundColor)
            throw std::runtime_error{"LAS RGB values were not written"};
        Check(reader, laszip_close_reader(reader), "Cannot close reader");
        destroy.Value = nullptr;
        Check(reader, laszip_destroy(reader), "Cannot destroy reader");
        std::cout << "Verified " << argv[1] << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
