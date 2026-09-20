#include "PoissonSphere.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace World
{
    namespace
    {
        [[nodiscard]] std::uint64_t SplitMix64(std::uint64_t& state) noexcept
        {
            state += 0x9e3779b97f4a7c15ull;
            auto value{state};
            value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ull;
            value = (value ^ (value >> 27u)) * 0x94d049bb133111ebull;
            return value ^ (value >> 31u);
        }

        [[nodiscard]] double UnitRandom(std::uint64_t& state) noexcept
        {
            return static_cast<double>(SplitMix64(state) >> 11u) * 0x1.0p-53;
        }

        [[nodiscard]] std::array<double, 9> MakeRotation(std::uint64_t seed)
        {
            const double first{UnitRandom(seed)};
            const double second{UnitRandom(seed)};
            const double third{UnitRandom(seed)};
            const double rootFirst{std::sqrt(1.0 - first)};
            const double rootSecond{std::sqrt(first)};
            const double angleSecond{2.0 * std::numbers::pi * second};
            const double angleThird{2.0 * std::numbers::pi * third};
            const double x{rootFirst * std::sin(angleSecond)};
            const double y{rootFirst * std::cos(angleSecond)};
            const double z{rootSecond * std::sin(angleThird)};
            const double w{rootSecond * std::cos(angleThird)};
            return {
                1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w), 2.0 * (x * z + y * w),
                2.0 * (x * y + z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w),
                2.0 * (x * z - y * w), 2.0 * (y * z + x * w), 1.0 - 2.0 * (x * x + y * y)};
        }
    } // namespace

    PoissonSphere::PoissonSphere(std::uint64_t count, std::uint64_t seed)
        : m_Count{count}, m_Rotation{MakeRotation(seed)}
    {
        if (count == 0)
            throw std::runtime_error{"Point count must be positive"};
    }

    void PoissonSphere::Generate(std::uint64_t first, std::span<Point3> output) const
    {
        if (first > m_Count || output.size() > m_Count - first)
            throw std::runtime_error{"Sample range exceeds point count"};

        constexpr std::uint64_t goldenTurn{0x61c8864680b583ebull};
        constexpr long double fullCircle{2.0L * std::numbers::pi_v<long double>};
        const long double count{static_cast<long double>(m_Count)};
        for (std::size_t offset = 0; offset < output.size(); ++offset)
        {
            const auto index{first + static_cast<std::uint64_t>(offset)};
            const long double height{1.0L - (2.0L * static_cast<long double>(index) + 1.0L) / count};
            const std::uint64_t phase{index * goldenTurn};
            const long double angle{static_cast<long double>(phase) * 0x1.0p-64L * fullCircle};
            const double z{static_cast<double>(height)};
            const double radial{std::sqrt(std::max(0.0, 1.0 - z * z))};
            const double x{radial * std::cos(static_cast<double>(angle))};
            const double y{radial * std::sin(static_cast<double>(angle))};
            output[offset] = {
                m_Rotation[0] * x + m_Rotation[1] * y + m_Rotation[2] * z,
                m_Rotation[3] * x + m_Rotation[4] * y + m_Rotation[5] * z,
                m_Rotation[6] * x + m_Rotation[7] * y + m_Rotation[8] * z};
        }
    }

    std::uint64_t PoissonSphere::Count() const noexcept
    {
        return m_Count;
    }
} // namespace World
