#include "onyx/core/pch.hpp"
#include "onyx/property/color.hpp"
#include "tkit/utils/debug.hpp"
#include "tkit/math/math.hpp"
#include <sstream>

namespace Onyx
{
static constexpr u8 toInt(const f32 p_Val)
{
    return static_cast<u8>(p_Val * 255.f);
}

static constexpr f32 toFloat(const u32 p_Val)
{
    constexpr f32 oneOver255 = 1.f / 255.f;
    return static_cast<f32>(p_Val) * oneOver255;
}

Color::Color(const f32 p_Val) : Color(p_Val, p_Val, p_Val, 1.f)
{
    TKIT_ASSERT(p_Val <= 1.f && p_Val >= 0.f, "[ONYX] Color floating values must be in the range [0, 1]");
}
Color::Color(const u32 p_Val) : Color(p_Val, p_Val, p_Val, 255u)
{
}

Color::Color(const f32v4 &p_RGBA) : RGBA(p_RGBA)
{
    TKIT_ASSERT(p_RGBA[0] <= 1.f && p_RGBA[0] >= 0.f, "[ONYX] Red value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGBA[1] <= 1.f && p_RGBA[1] >= 0.f, "[ONYX] Green value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGBA[2] <= 1.f && p_RGBA[2] >= 0.f, "[ONYX] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGBA[3] <= 1.f && p_RGBA[3] >= 0.f, "[ONYX] Alpha value must be in the range [0, 1]");
}
Color::Color(const f32v3 &p_RGB, const f32 p_Alpha) : RGBA(p_RGB, p_Alpha)
{
    TKIT_ASSERT(p_RGB[0] <= 1.f && p_RGB[0] >= 0.f, "[ONYX] Red value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGB[1] <= 1.f && p_RGB[1] >= 0.f, "[ONYX] Green value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGB[2] <= 1.f && p_RGB[2] >= 0.f, "[ONYX] Blue value must be in the range [0, 1]");
}

Color::Color(const f32 p_Red, const f32 p_Green, const f32 p_Blue, const f32 p_Alpha)
    : RGBA(p_Red, p_Green, p_Blue, p_Alpha)
{
    TKIT_ASSERT(p_Red <= 1.f && p_Red >= 0.f, "[ONYX] Red value must be in the range [0, 1]");
    TKIT_ASSERT(p_Green <= 1.f && p_Green >= 0.f, "[ONYX] Green value must be in the range [0, 1]");
    TKIT_ASSERT(p_Blue <= 1.f && p_Blue >= 0.f, "[ONYX] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(p_Alpha <= 1.f && p_Alpha >= 0.f, "[ONYX] Alpha value must be in the range [0, 1]");
}
Color::Color(const u32 p_Red, const u32 p_Green, const u32 p_Blue, const u32 p_Alpha)
    : RGBA(toFloat(p_Red), toFloat(p_Green), toFloat(p_Blue), toFloat(p_Alpha))
{
    TKIT_ASSERT(p_Red < 256, "[ONYX] Red value must be in the range [0, 255]");
    TKIT_ASSERT(p_Green < 256, "[ONYX] Green value must be in the range [0, 255]");
    TKIT_ASSERT(p_Blue < 256, "[ONYX] Blue value must be in the range [0, 255]");
    TKIT_ASSERT(p_Alpha < 256, "[ONYX] Alpha value must be in the range [0, 255]");
}

Color::Color(const Color &p_RGB, const f32 p_Alpha) : RGBA(f32v3(p_RGB.RGBA), p_Alpha)
{
    TKIT_ASSERT(p_Alpha <= 1.f && p_Alpha >= 0.f, "[ONYX] Alpha value must be in the range [0, 1]");
}

Color::Color(const Color &p_RGB, const u32 p_Alpha) : RGBA(f32v3(p_RGB.RGBA), toFloat(p_Alpha))
{
    TKIT_ASSERT(p_Alpha <= 255, "[ONYX] Alpha value must be in the range [0, 255]");
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

void Color::Red(const u32 p_Red)
{
    RGBA[0] = toFloat(p_Red);
}
void Color::Green(const u32 p_Green)
{
    RGBA[1] = toFloat(p_Green);
}
void Color::Blue(const u32 p_Blue)
{
    RGBA[2] = toFloat(p_Blue);
}
void Color::Alpha(const u32 p_Alpha)
{
    RGBA[3] = toFloat(p_Alpha);
}

u32 Color::Pack() const
{
    return Red() | Green() << 8 | Blue() << 16 | Alpha() << 24;
}
Color Color::Unpack(const u32 p_Packed)
{
    return Color{p_Packed & 0xFF, (p_Packed >> 8) & 0xFF, (p_Packed >> 16) & 0xFF, (p_Packed >> 24) & 0xFF};
}

template <> u32 Color::ToHexadecimal<u32>(const bool p_Alpha) const
{
    if (p_Alpha)
        return Red() << 24 | Green() << 16 | Blue() << 8 | Alpha();
    return Red() << 16 | Green() << 8 | Blue();
}

template <> std::string Color::ToHexadecimal<std::string>(const bool p_Alpha) const
{
    std::stringstream ss;
    ss << std::hex << ToHexadecimal<u32>(p_Alpha);
    std::string hex = ss.str();
    const u32 size = p_Alpha ? 8 : 6;
    while (hex.size() < size)
        hex = "0" + hex;

    return hex;
}

Color Color::FromHexadecimal(const u32 p_Hex, const bool p_Alpha)
{
    if (p_Alpha)
        return {p_Hex >> 24, (p_Hex >> 16) & 0xFF, (p_Hex >> 8) & 0xFF, p_Hex & 0xFF};
    return {p_Hex >> 16, (p_Hex >> 8) & 0xFF, p_Hex & 0xFF};
}
Color Color::FromHexadecimal(const std::string_view p_Hex)
{
    TKIT_ASSERT(p_Hex.size() == 6 || p_Hex.size() == 8, "[ONYX] Invalid hexadecimal color");
    u32 val;
    std::stringstream ss;
    ss << std::hex << p_Hex;
    ss >> val;
    return FromHexadecimal(val, p_Hex.size() == 8);
}

Color Color::FromString(const std::string &p_Color)
{
    TKIT_ASSERT(s_ColorMap.contains(p_Color), "[ONYX] Color '{}' not found", p_Color);
    return s_ColorMap.at(p_Color);
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

Gradient::Gradient(const TKit::Span<const Color> p_Span) : m_Colors(p_Span)
{
    TKIT_ASSERT(p_Span.GetSize() >= 2, "[ONYX] Gradient must have at least two colors");
}

Color Gradient::Evaluate(const f32 p_T) const
{
    TKIT_ASSERT(p_T >= 0.f && p_T <= 1.f, "[ONYX] Gradient evaluation parameter must be in the range [0, 1]");

    if (TKit::ApproachesZero(p_T))
        return m_Colors.GetFront();
    if (TKit::Approximately(p_T, 1.f))
        return m_Colors.GetBack();

    const f32 loc = p_T * (m_Colors.GetSize() - 1);
    const u32 index1 = static_cast<u32>(loc);

    const f32 t = loc - static_cast<f32>(index1);
    return Color{m_Colors[index1].RGBA * (1.f - t) + m_Colors[index1 + 1].RGBA * t};
}

} // namespace Onyx
