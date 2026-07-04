#include "pch.hpp"
#include "onyx/color.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/math/math.hpp"
#include "tkit/container/hash_map.hpp"

namespace Onyx
{
f32v4 Color::ToHSV(const f32v4 &rgba)
{
    const f32 r = rgba[0];
    const f32 g = rgba[1];
    const f32 b = rgba[2];
    const f32 a = rgba[3];

    const f32 cmax = Math::Max(r, Math::Max(g, b));
    const f32 cmin = Math::Min(r, Math::Min(g, b));
    const f32 delta = cmax - cmin;

    const f32 v = cmax;
    const f32 s = Math::ApproachesZero(cmax) ? 0.f : delta / cmax;

    f32 h;
    if (Math::ApproachesZero(delta))
        h = 0.f;
    else if (cmax == r)
        h = (g - b) / (6.f * delta);
    else if (cmax == g)
        h = (2.f + (b - r) / delta) / 6.f;
    else
        h = (4.f + (r - g) / delta) / 6.f;

    if (h < 0.f)
        ++h;

    return f32v4{h, s, v, a};
}

Color Color::FromHSV(const f32v4 &rgba)
{
    const f32 h = rgba[0];
    const f32 s = rgba[1];
    const f32 v = rgba[2];
    const f32 a = rgba[3];

    const f32 c = v * s;
    const f32 h6 = 6.f * h;
    const f32 x = c * (1.f - Math::Absolute(Math::Modulo(h6, 2.f) - 1.f));
    const f32 m = v - c;

    const f32 c0 = c + m;
    const f32 c1 = x + m;
    const f32 c2 = m;

    const u32 sector = u32(h6);
    switch (sector)
    {
    case 0:
        return f32v4{c0, c1, c2, a};
    case 1:
        return f32v4{c1, c0, c2, a};
    case 2:
        return f32v4{c2, c0, c1, a};
    case 3:
        return f32v4{c2, c1, c0, a};
    case 4:
        return f32v4{c1, c2, c0, a};
    case 5:
        return f32v4{c0, c2, c1, a};
    default:
        return f32v4{c0, c1, c2, a};
    }
}

Color Color::FromHexadecimal(const u32 hex, const bool alpha)
{
    if (alpha)
        return {hex >> 24, (hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF};
    return {hex >> 16, (hex >> 8) & 0xFF, hex & 0xFF};
}
Color Color::FromHexadecimal(TKit::StringView hex)
{
    return FromHexadecimal(u32(std::strtoul(hex.begin(), nullptr, 16)), hex.GetSize() == 8);
}

Color Color::FromString(const TKit::String &color)
{
    static const TKit::StaticHashMap<TKit::String, Color *, 128> s_ColorMap{{"red", &Color_Red},
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
    TKIT_ASSERT(s_ColorMap.Contains(color), "[ONYX][COLOR] Color '{}' not found", color);
    return *s_ColorMap[color];
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

    if (Math::ApproachesZero(t))
        return m_Colors.GetFront();
    if (Math::Approximately(t, 1.f))
        return m_Colors.GetBack();

    const f32 loc = t * (m_Colors.GetSize() - 1);
    const u32 index1 = u32(loc);

    const f32 tt = loc - f32(index1);
    return Color{m_Colors[index1].rgba * (1.f - tt) + m_Colors[index1 + 1].rgba * tt};
}

} // namespace Onyx
