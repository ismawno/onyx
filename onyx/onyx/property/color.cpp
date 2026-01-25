#include "onyx/core/pch.hpp"
#include "onyx/property/color.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/math/math.hpp"
#include <sstream>

namespace Onyx
{
static constexpr u8 toInt(const f32 val)
{
    return static_cast<u8>(val * 255.f);
}

static constexpr f32 toFloat(const u32 val)
{
    constexpr f32 oneOver255 = 1.f / 255.f;
    return static_cast<f32>(val) * oneOver255;
}

Color::Color(const f32 val) : Color(val, val, val, 1.f)
{
    TKIT_ASSERT(val <= 1.f && val >= 0.f, "[ONYX][COLOR] Color floating values must be in the range [0, 1]");
}
Color::Color(const u32 val) : Color(val, val, val, 255u)
{
}

Color::Color(const f32v4 &rgba) : RGBA(rgba)
{
    TKIT_ASSERT(rgba[0] <= 1.f && rgba[0] >= 0.f, "[ONYX][COLOR] Red value must be in the range [0, 1]");
    TKIT_ASSERT(rgba[1] <= 1.f && rgba[1] >= 0.f, "[ONYX][COLOR] Green value must be in the range [0, 1]");
    TKIT_ASSERT(rgba[2] <= 1.f && rgba[2] >= 0.f, "[ONYX][COLOR] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(rgba[3] <= 1.f && rgba[3] >= 0.f, "[ONYX][COLOR] Alpha value must be in the range [0, 1]");
}
Color::Color(const f32v3 &rgb, const f32 alpha) : RGBA(rgb, alpha)
{
    TKIT_ASSERT(rgb[0] <= 1.f && rgb[0] >= 0.f, "[ONYX][COLOR] Red value must be in the range [0, 1]");
    TKIT_ASSERT(rgb[1] <= 1.f && rgb[1] >= 0.f, "[ONYX][COLOR] Green value must be in the range [0, 1]");
    TKIT_ASSERT(rgb[2] <= 1.f && rgb[2] >= 0.f, "[ONYX][COLOR] Blue value must be in the range [0, 1]");
}

Color::Color(const f32 red, const f32 green, const f32 blue, const f32 alpha) : RGBA(red, green, blue, alpha)
{
    TKIT_ASSERT(red <= 1.f && red >= 0.f, "[ONYX][COLOR] Red value must be in the range [0, 1]");
    TKIT_ASSERT(green <= 1.f && green >= 0.f, "[ONYX][COLOR] Green value must be in the range [0, 1]");
    TKIT_ASSERT(blue <= 1.f && blue >= 0.f, "[ONYX][COLOR] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(alpha <= 1.f && alpha >= 0.f, "[ONYX][COLOR] Alpha value must be in the range [0, 1]");
}
Color::Color(const u32 red, const u32 green, const u32 blue, const u32 alpha)
    : RGBA(toFloat(red), toFloat(green), toFloat(blue), toFloat(alpha))
{
    TKIT_ASSERT(red < 256, "[ONYX][COLOR] Red value must be in the range [0, 255]");
    TKIT_ASSERT(green < 256, "[ONYX][COLOR] Green value must be in the range [0, 255]");
    TKIT_ASSERT(blue < 256, "[ONYX][COLOR] Blue value must be in the range [0, 255]");
    TKIT_ASSERT(alpha < 256, "[ONYX][COLOR] Alpha value must be in the range [0, 255]");
}

Color::Color(const Color &rgb, const f32 alpha) : RGBA(f32v3(rgb.RGBA), alpha)
{
    TKIT_ASSERT(alpha <= 1.f && alpha >= 0.f, "[ONYX][COLOR] Alpha value must be in the range [0, 1]");
}

Color::Color(const Color &rgb, const u32 alpha) : RGBA(f32v3(rgb.RGBA), toFloat(alpha))
{
    TKIT_ASSERT(alpha <= 255, "[ONYX][COLOR] Alpha value must be in the range [0, 255]");
}

u8 Color::Red() const
{
    return toInt(RGBA[0]);
}
u8 Color::Green() const
{
    return toInt(RGBA[1]);
}
u8 Color::Blue() const
{
    return toInt(RGBA[2]);
}
u8 Color::Alpha() const
{
    return toInt(RGBA[3]);
}

void Color::Red(const u32 red)
{
    RGBA[0] = toFloat(red);
}
void Color::Green(const u32 green)
{
    RGBA[1] = toFloat(green);
}
void Color::Blue(const u32 blue)
{
    RGBA[2] = toFloat(blue);
}
void Color::Alpha(const u32 alpha)
{
    RGBA[3] = toFloat(alpha);
}

u32 Color::Pack() const
{
    return Red() | Green() << 8 | Blue() << 16 | Alpha() << 24;
}
Color Color::Unpack(const u32 packed)
{
    return Color{packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF};
}

template <> u32 Color::ToHexadecimal<u32>(const bool alpha) const
{
    if (alpha)
        return Red() << 24 | Green() << 16 | Blue() << 8 | Alpha();
    return Red() << 16 | Green() << 8 | Blue();
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
    return s_ColorMap.at(color);
}

const f32 *Color::GetData() const
{
    return Math::AsPointer(RGBA);
}
f32 *Color::GetData()
{
    return Math::AsPointer(RGBA);
}

Color::operator const f32v4 &() const
{
    return RGBA;
}
Color::operator const f32v3 &() const
{
    return RGB;
}

Color &Color::operator+=(const Color &rhs)
{
    RGB = Math::Clamp(RGB + rhs.RGB, 0.f, 1.f);
    return *this;
}
Color &Color::operator-=(const Color &rhs)
{
    RGB = Math::Clamp(RGB - rhs.RGB, 0.f, 1.f);
    return *this;
}

Color &Color::operator*=(const Color &rhs)
{
    RGB = Math::Clamp(RGB * rhs.RGB, 0.f, 1.f);
    return *this;
}

Color &Color::operator/=(const Color &rhs)
{
    RGB = Math::Clamp(RGB / rhs.RGB, 0.f, 1.f);
    return *this;
}

const Color Color::RED{255u, 0u, 0u};
const Color Color::GREEN{0u, 255u, 0u};
const Color Color::BLUE{0u, 0u, 255u};
const Color Color::MAGENTA{255u, 0u, 255u};
const Color Color::CYAN{0u, 255u, 255u};
const Color Color::ORANGE{255u, 165u, 0u};
const Color Color::YELLOW{255u, 255u, 0u};
const Color Color::BLACK{0u};
const Color Color::PINK{255u, 192u, 203u};
const Color Color::PURPLE{191u, 64u, 191u};
const Color Color::WHITE{255u};
const Color Color::TRANSPARENT{WHITE, 0u};

const std::unordered_map<std::string, Color> Color::s_ColorMap{
    {"red", RED},   {"green", GREEN},   {"blue", BLUE},     {"magenta", MAGENTA},
    {"cyan", CYAN}, {"orange", ORANGE}, {"yellow", YELLOW}, {"black", BLACK},
    {"pink", PINK}, {"purple", PURPLE}, {"white", WHITE},   {"transparent", TRANSPARENT}};

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
    const u32 index1 = static_cast<u32>(loc);

    const f32 tt = loc - static_cast<f32>(index1);
    return Color{m_Colors[index1].RGBA * (1.f - tt) + m_Colors[index1 + 1].RGBA * tt};
}

} // namespace Onyx
