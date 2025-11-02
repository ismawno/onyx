#include "onyx/core/pch.hpp"
#include "onyx/property/color.hpp"
#include "tkit/utils/logging.hpp"
#include "tkit/math/math.hpp"
#include <sstream>

namespace Onyx
{
static u8 toInt(const f32 p_Val)
{
    return static_cast<u8>(p_Val * 255.f);
}

static f32 toFloat(const u8 p_Val)
{
    constexpr f32 oneOver255 = 1.f / 255.f;
    return static_cast<f32>(p_Val) * oneOver255;
}
static f32 toFloat(const u32 p_Val)
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
Color::Color(const u8 p_Val) : Color(p_Val, p_Val, p_Val, 255)
{
}

Color::Color(const fvec4 &p_RGBA) : RGBA(p_RGBA)
{
    TKIT_ASSERT(p_RGBA.r <= 1.f && p_RGBA.r >= 0.f, "[ONYX] Red value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGBA.g <= 1.f && p_RGBA.g >= 0.f, "[ONYX] Green value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGBA.b <= 1.f && p_RGBA.b >= 0.f, "[ONYX] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGBA.a <= 1.f && p_RGBA.a >= 0.f, "[ONYX] Alpha value must be in the range [0, 1]");
}
Color::Color(const fvec3 &p_RGB, const f32 p_Alpha) : RGBA(p_RGB, p_Alpha)
{
    TKIT_ASSERT(p_RGB.r <= 1.f && p_RGB.r >= 0.f, "[ONYX] Red value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGB.g <= 1.f && p_RGB.g >= 0.f, "[ONYX] Green value must be in the range [0, 1]");
    TKIT_ASSERT(p_RGB.b <= 1.f && p_RGB.b >= 0.f, "[ONYX] Blue value must be in the range [0, 1]");
}

Color::Color(const f32 r, const f32 g, const f32 b, const f32 a) : RGBA(r, g, b, a)
{
    TKIT_ASSERT(r <= 1.f && r >= 0.f, "[ONYX] Red value must be in the range [0, 1]");
    TKIT_ASSERT(g <= 1.f && g >= 0.f, "[ONYX] Green value must be in the range [0, 1]");
    TKIT_ASSERT(b <= 1.f && b >= 0.f, "[ONYX] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(a <= 1.f && a >= 0.f, "[ONYX] Alpha value must be in the range [0, 1]");
}
Color::Color(const u32 r, const u32 g, const u32 b, const u32 a) : RGBA(toFloat(r), toFloat(g), toFloat(b), toFloat(a))
{
    TKIT_ASSERT(r < 256, "[ONYX] Red value must be in the range [0, 255]");
    TKIT_ASSERT(g < 256, "[ONYX] Green value must be in the range [0, 255]");
    TKIT_ASSERT(b < 256, "[ONYX] Blue value must be in the range [0, 255]");
    TKIT_ASSERT(a < 256, "[ONYX] Alpha value must be in the range [0, 255]");
}
Color::Color(const u8 r, const u8 g, const u8 b, const u8 a) : RGBA(toFloat(r), toFloat(g), toFloat(b), toFloat(a))
{
    // No asserts needed: u8 is always in the range [0, 255]
}

Color::Color(const Color &p_RGB, const f32 p_Alpha) : RGBA(fvec3(p_RGB.RGBA), p_Alpha)
{
    TKIT_ASSERT(p_Alpha <= 1.f && p_Alpha >= 0.f, "[ONYX] Alpha value must be in the range [0, 1]");
}

Color::Color(const Color &p_RGB, const u32 p_Alpha) : RGBA(fvec3(p_RGB.RGBA), toFloat(p_Alpha))
{
    TKIT_ASSERT(p_Alpha < 256, "[ONYX] Alpha value must be in the range [0, 255]");
}
Color::Color(const Color &p_RGB, const u8 p_Alpha) : RGBA(fvec3(p_RGB.RGBA), toFloat(p_Alpha))
{
    // No asserts needed: u8 is always in the range [0, 255]
}

u8 Color::Red() const
{
    return toInt(RGBA.r);
}
u8 Color::Green() const
{
    return toInt(RGBA.g);
}
u8 Color::Blue() const
{
    return toInt(RGBA.b);
}
u8 Color::Alpha() const
{
    return toInt(RGBA.a);
}

void Color::Red(const u8 p_Red)
{
    RGBA.r = toFloat(p_Red);
}
void Color::Green(const u8 p_Green)
{
    RGBA.g = toFloat(p_Green);
}
void Color::Blue(const u8 p_Blue)
{
    RGBA.b = toFloat(p_Blue);
}
void Color::Alpha(const u8 p_Alpha)
{
    RGBA.a = toFloat(p_Alpha);
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

const f32 *Color::AsPointer() const
{
    return glm::value_ptr(RGBA);
}
f32 *Color::AsPointer()
{
    return glm::value_ptr(RGBA);
}

Color::operator const fvec4 &() const
{
    return RGBA;
}
Color::operator const fvec3 &() const
{
    return RGB;
}

Color &Color::operator+=(const Color &rhs)
{
    RGB = glm::clamp(RGB + rhs.RGB, 0.f, 1.f);
    return *this;
}
Color &Color::operator-=(const Color &rhs)
{
    RGB = glm::clamp(RGB - rhs.RGB, 0.f, 1.f);
    return *this;
}

Color &Color::operator*=(const Color &rhs)
{
    RGB = glm::clamp(RGB * rhs.RGB, 0.f, 1.f);
    return *this;
}

Color &Color::operator/=(const Color &rhs)
{
    RGB = glm::clamp(RGB / rhs.RGB, 0.f, 1.f);
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

const TKit::HashMap<std::string, Color> Color::s_ColorMap{
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
