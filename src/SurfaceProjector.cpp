#include "SurfaceProjector.h"

#include <algorithm>
#include <cmath>

namespace World
{
    namespace
    {
        [[nodiscard]] Point3 Cross(const Point3& a, const Point3& b) noexcept
        {
            return {a.Y * b.Z - a.Z * b.Y,
                a.Z * b.X - a.X * b.Z,
                a.X * b.Y - a.Y * b.X};
        }

        [[nodiscard]] Point3 Normalize(const Point3& point) noexcept
        {
            const double inverseLength{1.0 / std::sqrt(
                point.X * point.X + point.Y * point.Y + point.Z * point.Z)};
            return {point.X * inverseLength, point.Y * inverseLength, point.Z * inverseLength};
        }

        [[nodiscard]] Point3 OffsetDirection(const Point3& direction,
            const Point3& tangent, double angle) noexcept
        {
            const double radial{std::cos(angle)};
            const double offset{std::sin(angle)};
            return {direction.X * radial + tangent.X * offset,
                direction.Y * radial + tangent.Y * offset,
                direction.Z * radial + tangent.Z * offset};
        }
    } // namespace

    SurfaceProjector::SurfaceProjector(const Heightmap& heightmap, double radius,
        double heightMin, double heightMax, double heightScale)
        : m_Heightmap{heightmap}, m_Radius{radius}, m_HeightMin{heightMin},
          m_HeightMax{heightMax}, m_HeightScale{heightScale},
          m_NormalStep{heightmap.NormalSampleStep()}
    {
    }

    double SurfaceProjector::Elevation(const Point3& direction) const
    {
        return std::lerp(m_HeightMin, m_HeightMax,
            m_Heightmap.Sample(direction)) * m_HeightScale;
    }

    void SurfaceProjector::Project(const Point3& direction, Point3& position, Point3* normal) const
    {
        const double elevation{Elevation(direction)};
        const double radial{m_Radius + elevation};
        position = {direction.X * radial, direction.Y * radial, direction.Z * radial};
        if (normal == nullptr)
            return;

        const Point3 reference{std::abs(direction.Z) < 0.9 ? Point3{0.0, 0.0, 1.0}
                                                            : Point3{0.0, 1.0, 0.0}};
        const Point3 firstTangent{Normalize(Cross(reference, direction))};
        const Point3 secondTangent{Cross(direction, firstTangent)};
        const double firstDerivative{
            (Elevation(OffsetDirection(direction, firstTangent, m_NormalStep))
                - Elevation(OffsetDirection(direction, firstTangent, -m_NormalStep)))
            / (2.0 * m_NormalStep)};
        const double secondDerivative{
            (Elevation(OffsetDirection(direction, secondTangent, m_NormalStep))
                - Elevation(OffsetDirection(direction, secondTangent, -m_NormalStep)))
            / (2.0 * m_NormalStep)};
        *normal = Normalize({direction.X - (firstDerivative * firstTangent.X
                + secondDerivative * secondTangent.X) / radial,
            direction.Y - (firstDerivative * firstTangent.Y
                + secondDerivative * secondTangent.Y) / radial,
            direction.Z - (firstDerivative * firstTangent.Z
                + secondDerivative * secondTangent.Z) / radial});
    }
} // namespace World
