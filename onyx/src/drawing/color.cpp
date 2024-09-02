#include "core/pch.hpp"
#include "onyx/drawing/color.hpp"
#include "kit/core/logging.hpp"
#include <sstream>

namespace ONYX
{
static u8 toInt(const f32 p_Val) noexcept
{
    return static_cast<u8>(p_Val * 255.f);
}

static f32 toFloat(const u8 p_Val) noexcept
{
    constexpr f32 oneOver255 = 1.f / 255.f;
    return static_cast<f32>(p_Val) * oneOver255;
}
static f32 toFloat(const u32 p_Val) noexcept
{
    constexpr f32 oneOver255 = 1.f / 255.f;
    return static_cast<f32>(p_Val) * oneOver255;
}

Color::Color(const f32 p_Val) noexcept : Color(p_Val, p_Val, p_Val, 1.f)
{
    KIT_ASSERT(p_Val <= 1.f && p_Val >= 0.f, "Color floating values must be in the range [0, 1]");
}
Color::Color(const u32 p_Val) noexcept : Color(p_Val, p_Val, p_Val, 255u)
{
}
Color::Color(const u8 p_Val) noexcept : Color(p_Val, p_Val, p_Val, 255)
{
}

Color::Color(const vec4 &p_RGBA) noexcept : RGBA(p_RGBA)
{
    KIT_ASSERT(p_RGBA.r <= 1.f && p_RGBA.r >= 0.f, "Red value must be in the range [0, 1]");
    KIT_ASSERT(p_RGBA.g <= 1.f && p_RGBA.g >= 0.f, "Green value must be in the range [0, 1]");
    KIT_ASSERT(p_RGBA.b <= 1.f && p_RGBA.b >= 0.f, "Blue value must be in the range [0, 1]");
    KIT_ASSERT(p_RGBA.a <= 1.f && p_RGBA.a >= 0.f, "Alpha value must be in the range [0, 1]");
}
Color::Color(const vec3 &p_RGB, const f32 p_Alpha) noexcept : RGBA(p_RGB, p_Alpha)
{
    KIT_ASSERT(p_RGB.r <= 1.f && p_RGB.r >= 0.f, "Red value must be in the range [0, 1]");
    KIT_ASSERT(p_RGB.g <= 1.f && p_RGB.g >= 0.f, "Green value must be in the range [0, 1]");
    KIT_ASSERT(p_RGB.b <= 1.f && p_RGB.b >= 0.f, "Blue value must be in the range [0, 1]");
}

Color::Color(const f32 r, const f32 g, const f32 b, const f32 a) noexcept : RGBA(r, g, b, a)
{
    KIT_ASSERT(r <= 1.f && r >= 0.f, "Red value must be in the range [0, 1]");
    KIT_ASSERT(g <= 1.f && g >= 0.f, "Green value must be in the range [0, 1]");
    KIT_ASSERT(b <= 1.f && b >= 0.f, "Blue value must be in the range [0, 1]");
    KIT_ASSERT(a <= 1.f && a >= 0.f, "Alpha value must be in the range [0, 1]");
}
Color::Color(const u32 r, const u32 g, const u32 b, const u32 a) noexcept
    : RGBA(toFloat(r), toFloat(g), toFloat(b), toFloat(a))
{
    KIT_ASSERT(r < 256, "Red value must be in the range [0, 255]");
    KIT_ASSERT(g < 256, "Green value must be in the range [0, 255]");
    KIT_ASSERT(b < 256, "Blue value must be in the range [0, 255]");
    KIT_ASSERT(a < 256, "Alpha value must be in the range [0, 255]");
}
Color::Color(const u8 r, const u8 g, const u8 b, const u8 a) noexcept
    : RGBA(toFloat(r), toFloat(g), toFloat(b), toFloat(a))
{
    // No asserts needed: u8 is always in the range [0, 255]
}

Color::Color(const Color &p_RGB, const f32 p_Alpha) noexcept : RGBA(vec3(p_RGB.RGBA), p_Alpha)
{
    KIT_ASSERT(p_Alpha <= 1.f && p_Alpha >= 0.f, "Alpha value must be in the range [0, 1]");
}

Color::Color(const Color &p_RGB, const u32 p_Alpha) noexcept : RGBA(vec3(p_RGB.RGBA), toFloat(p_Alpha))
{
    KIT_ASSERT(p_Alpha < 256, "Alpha value must be in the range [0, 255]");
}
Color::Color(const Color &p_RGB, const u8 p_Alpha) noexcept : RGBA(vec3(p_RGB.RGBA), toFloat(p_Alpha))
{
    // No asserts needed: u8 is always in the range [0, 255]
}

u8 Color::Red() const noexcept
{
    return toInt(RGBA.r);
}
u8 Color::Green() const noexcept
{
    return toInt(RGBA.g);
}
u8 Color::Blue() const noexcept
{
    return toInt(RGBA.b);
}
u8 Color::Alpha() const noexcept
{
    return toInt(RGBA.a);
}

void Color::Red(const u8 p_Red) noexcept
{
    RGBA.r = toFloat(p_Red);
}
void Color::Green(const u8 p_Green) noexcept
{
    RGBA.g = toFloat(p_Green);
}
void Color::Blue(const u8 p_Blue) noexcept
{
    RGBA.b = toFloat(p_Blue);
}
void Color::Alpha(const u8 p_Alpha) noexcept
{
    RGBA.a = toFloat(p_Alpha);
}

template <> u32 Color::ToHexadecimal<u32>(const bool p_Alpha) const noexcept
{
    if (p_Alpha)
        return Red() << 24 | Green() << 16 | Blue() << 8 | Alpha();
    return Red() << 16 | Green() << 8 | Blue();
}

template <> std::string Color::ToHexadecimal<std::string>(const bool p_Alpha) const noexcept
{
    std::stringstream ss;
    ss << std::hex << ToHexadecimal<u32>(p_Alpha);
    std::string hex = ss.str();
    if (p_Alpha)
        while (hex.size() < 8)
            hex = "0" + hex;
    else
        while (hex.size() < 6)
            hex = "0" + hex;
    return hex;
}

Color Color::FromHexadecimal(const u32 p_Hex, const bool p_Alpha) noexcept
{
    if (p_Alpha)
        return {p_Hex >> 24, (p_Hex >> 16) & 0xFF, (p_Hex >> 8) & 0xFF, p_Hex & 0xFF};
    return {p_Hex >> 16, (p_Hex >> 8) & 0xFF, p_Hex & 0xFF};
}
Color Color::FromHexadecimal(const std::string &p_Hex, const bool p_Alpha) noexcept
{
    u32 val;
    std::stringstream ss;
    ss << std::hex << p_Hex;
    ss >> val;
    return FromHexadecimal(val, p_Alpha);
}

Color Color::FromString(const std::string_view p_Color) noexcept
{
    const auto it = s_ColorMap.find(p_Color);
    if (it != s_ColorMap.end())
        return it->second;
    return Color::WHITE;
}

const f32 *Color::Pointer() const noexcept
{
    return glm::value_ptr(RGBA);
}
f32 *Color::Pointer() noexcept
{
    return glm::value_ptr(RGBA);
}

Color::operator const vec4 &() const noexcept
{
    return RGBA;
}
Color::operator const vec3 &() const noexcept
{
    return RGB;
}

Color &Color::operator+=(const Color &rhs) noexcept
{
    RGB = glm::clamp(RGB + rhs.RGB, 0.f, 1.f);
    return *this;
}
Color &Color::operator-=(const Color &rhs) noexcept
{
    RGB = glm::clamp(RGB - rhs.RGB, 0.f, 1.f);
    return *this;
}

Color &Color::operator*=(const Color &rhs) noexcept
{
    RGB = glm::clamp(RGB * rhs.RGB, 0.f, 1.f);
    return *this;
}

Color &Color::operator/=(const Color &rhs) noexcept
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

const HashMap<std::string, Color> Color::s_ColorMap{
    {"red", RED},   {"green", GREEN},   {"blue", BLUE},     {"magenta", MAGENTA},
    {"cyan", CYAN}, {"orange", ORANGE}, {"yellow", YELLOW}, {"black", BLACK},
    {"pink", PINK}, {"purple", PURPLE}, {"white", WHITE},   {"transparent", TRANSPARENT}};

} // namespace ONYX