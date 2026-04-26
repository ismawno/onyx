#include "onyx/core/pch.hpp"
#include "onyx/property/color.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/math/math.hpp"
#include <sstream>

namespace Onyx
{

static const std::unordered_map<std::string, Color *> s_ColorMap{{"red", &Color_Red},
                                                                 {"green", &Color_Green},
                                                                 {"blue", &Color_Blue},
                                                                 {"magenta", &Color_Magenta},
                                                                 {"cyan", &Color_Cyan},
                                                                 {"orange", &Color_Orange},
                                                                 {"yellow", &Color_Yellow},
                                                                 {"black", &Color_Black},
                                                                 {"pink", &Color_Pink},
                                                                 {"purple", &Color_Purple},
                                                                 {"white", &Color_White},
                                                                 {"transparent", &Color_Transparent}

};

template <> u32 Color::ToHexadecimal<u32>(const bool alpha) const
{
    if (alpha)
        return r() << 24 | g() << 16 | b() << 8 | a();
    return r() << 16 | g() << 8 | b();
}

template <> std::string Color::ToHexadecimal<std::string>(const bool alpha) const
{
    std::stringstream ss;
    ss << std::hex << ToHexadecimal<u32>(alpha);
    std::string hex = ss.str();
    const u32 size = alpha ? 8 : 6;
    while (hex.size() < size)
        hex = "0" + hex;

    return hex;
}

Color Color::FromHexadecimal(const u32 hex, const bool alpha)
{
    if (alpha)
        return {hex >> 24, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF};
    return {hex >> 16, (hex >> 8) & 0xFF, hex & 0xFF};
}
Color Color::FromHexadecimal(const std::string_view hex)
{
    TKIT_ASSERT(hex.size() == 6 || hex.size() == 8, "[ONYX][COLOR] Invalid hexadecimal color");
    u32 val;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> val;
    return FromHexadecimal(val, hex.size() == 8);
}

Color Color::FromString(const std::string &color)
{
    TKIT_ASSERT(s_ColorMap.contains(color), "[ONYX][COLOR] Color '{}' not found", color);
    return *s_ColorMap.at(color);
}

Color::operator const f32v4 &() const
{
    return rgba;
}
Color::operator const f32v3 &() const
{
    return rgb;
}

Color &Color::operator+=(const Color &rhs)
{
    rgb = Math::Clamp(rgb + rhs.rgb, 0.f, 1.f);
    return *this;
}
Color &Color::operator-=(const Color &rhs)
{
    rgb = Math::Clamp(rgb - rhs.rgb, 0.f, 1.f);
    return *this;
}

Color &Color::operator*=(const Color &rhs)
{
    rgb = Math::Clamp(rgb * rhs.rgb, 0.f, 1.f);
    return *this;
}

Color &Color::operator/=(const Color &rhs)
{
    rgb = Math::Clamp(rgb / rhs.rgb, 0.f, 1.f);
    return *this;
}

Gradient::Gradient(const TKit::Span<const Color> span) : m_Colors(span)
{
    TKIT_ASSERT(span.GetSize() >= 2, "[ONYX][GRADIENT] Gradient must have at least two colors");
}

Color Gradient::Evaluate(const f32 t) const
{
    TKIT_ASSERT(t >= 0.f && t <= 1.f, "[ONYX][GRADIENT] Gradient evaluation parameter must be in the range [0, 1]");

    if (TKit::ApproachesZero(t))
        return m_Colors.GetFront();
    if (TKit::Approximately(t, 1.f))
        return m_Colors.GetBack();

    const f32 loc = t * (m_Colors.GetSize() - 1);
    const u32 index1 = u32(loc);

    const f32 tt = loc - f32(index1);
    return Color{m_Colors[index1].rgba * (1.f - tt) + m_Colors[index1 + 1].rgba * tt};
}

} // namespace Onyx
