#pragma once

#include "Geometry.h"

#include <span>

namespace World
{
    class PointWriter
    {
    public:
        virtual ~PointWriter() = default;

        virtual void Write(std::span<const Point3> points,
            std::span<const Point3> normals = {},
            std::span<const Color3> colors = {}) = 0;
        virtual void Finish() = 0;
    };
} // namespace World
