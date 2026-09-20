#pragma once

#include "Geometry.h"
#include "Heightmap.h"

namespace World
{
    /**
     * \class SurfaceProjector
     * \brief Projects unit directions onto a radially displaced height surface.
     *
     * Optional normals use central heightmap derivatives along two orthogonal
     * great-circle tangents. Returned normals are unit length and point outward.
     * The object is immutable after construction and safe for concurrent use.
     */
    class SurfaceProjector final
    {
    public:
        SurfaceProjector(const Heightmap& heightmap, double radius,
            double heightMin, double heightMax, double heightScale);

        /**
         * \brief Project one unit direction and optionally calculate its normal.
         * \param[in] direction Unit direction on the reference sphere.
         * \param[out] position Displaced Cartesian position in metres.
         * \param[out] normal Optional unit surface normal.
         */
        void Project(const Point3& direction, Point3& position, Point3* normal) const;

    private:
        const Heightmap& m_Heightmap;
        double m_Radius{};
        double m_HeightMin{};
        double m_HeightMax{};
        double m_HeightScale{};
        double m_NormalStep{};

        [[nodiscard]] double Elevation(const Point3& direction) const;
    };
} // namespace World
