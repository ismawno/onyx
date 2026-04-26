#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/math.hpp"
#include "tkit/container/span.hpp"
#include "tkit/utils/debug.hpp"

namespace Onyx::Detail
{
template <Numeric T> constexpr T ToType(const f32 val)
{
    if constexpr (Float<T>)
        return val;
    else
        return T(val * 255.f);
}
template <Numeric T> constexpr f32 FromType(const T val)
{
    if constexpr (Float<T>)
        return val;
    else
        return T(val) / 255.f;
}
constexpr f32 ToLinear(const f32 c)
{
    return c <= 0.04045f ? c / 12.92f : Math::Pow((c + 0.055f) / 1.055f, 2.4f);
}
constexpr f32 ToSrgb(const f32 c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * Math::Pow(c, 1.0f / 2.4f) - 0.055f;
}
#ifdef TKIT_ENABLE_ASSERTS
inline void CheckRgba(const f32v4 &rgba)
{
    TKIT_ASSERT(rgba[0] <= 1.f && rgba[0] >= 0.f, "[ONYX][COLOR] Red value must be in the range [0, 1]");
    TKIT_ASSERT(rgba[1] <= 1.f && rgba[1] >= 0.f, "[ONYX][COLOR] Green value must be in the range [0, 1]");
    TKIT_ASSERT(rgba[2] <= 1.f && rgba[2] >= 0.f, "[ONYX][COLOR] Blue value must be in the range [0, 1]");
    TKIT_ASSERT(rgba[3] <= 1.f && rgba[3] >= 0.f, "[ONYX][COLOR] Alpha value must be in the range [0, 1]");
}
#    define CHECK_RGBA(rgba) Detail::CheckRgba(rgba)
#else
#    define CHECK_RGBA(rgba)
#endif
} // namespace Onyx::Detail

namespace Onyx
{
struct Color
{
    constexpr Color(const f32v4 &rgba) : rgba(rgba)
    {
        CHECK_RGBA(rgba);
    }

    constexpr Color(const f32v3 &rgb, const f32 alpha = 1.f) : rgba(rgb, alpha)
    {
        CHECK_RGBA(rgba);
    }

    constexpr Color(const f32 rgb = 1.f, const f32 alpha = 1.f) : Color(rgb, rgb, rgb, alpha)
    {
    }
    constexpr Color(const u32 rgb, const u32 alpha = 255) : Color(rgb, rgb, rgb, alpha)
    {
    }

    constexpr Color(const f32 red, const f32 green, const f32 blue, const f32 alpha = 1.f)
        : rgba(red, green, blue, alpha)
    {
        CHECK_RGBA(rgba);
    }
    constexpr Color(const u32 red, const u32 green, const u32 blue, const u32 alpha = 255)
        : rgba(Detail::FromType(red), Detail::FromType(green), Detail::FromType(blue), Detail::FromType(alpha))
    {
        CHECK_RGBA(rgba);
    }

    constexpr Color(const Color &rgb, const f32 alpha) : rgba(rgb.rgb, alpha)
    {
        CHECK_RGBA(rgba);
    }

    constexpr Color(const Color &rgb, const u32 alpha) : rgba(rgb.rgb, Detail::FromType(alpha))
    {
        CHECK_RGBA(rgba);
    }

    union {
        f32v4 rgba;
        f32v3 rgb;
    };

    template <Numeric T = u8> T r() const
    {
        return Detail::ToType<T>(rgba[0]);
    }
    template <Numeric T = u8> T g() const
    {
        return Detail::ToType<T>(rgba[1]);
    }
    template <Numeric T = u8> T b() const
    {
        return Detail::ToType<T>(rgba[2]);
    }
    template <Numeric T = u8> T a() const
    {
        return Detail::ToType<T>(rgba[3]);
    }

    template <Numeric T> void r(const T val) const
    {
        rgba[0] = Detail::FromType(val);
    }
    template <Numeric T> void g(const T val) const
    {
        rgba[1] = Detail::FromType(val);
    }
    template <Numeric T> void b(const T val) const
    {
        rgba[2] = Detail::FromType(val);
    }
    template <Numeric T> void a(const T val) const
    {
        rgba[3] = Detail::FromType(val);
    }

    u32 Pack() const
    {
        return r() | g() << 8 | b() << 16 | a() << 24;
    }
    static Color Unpack(const u32 packed)
    {
        return Color{packed & 0xFF, (packed >> 8) & 0xFF, (packed >> 16) & 0xFF, (packed >> 24) & 0xFF};
    }

    template <typename T> T ToHexadecimal(bool alpha = true) const;

    static Color FromHexadecimal(u32 hex, bool alpha = true);
    static Color FromHexadecimal(std::string_view hex);

    static Color FromString(const std::string &color);

    const f32 *GetData() const
    {
        return Math::AsPointer(rgba);
    }
    f32 *GetData()
    {
        return Math::AsPointer(rgba);
    }

    Color ToLinear() const
    {
        return Color(Detail::ToLinear(rgba[0]), Detail::ToLinear(rgba[1]), Detail::ToLinear(rgba[2]), rgba[3]);
    }
    Color ToSrgb() const
    {
        return Color(Detail::ToSrgb(rgba[0]), Detail::ToSrgb(rgba[1]), Detail::ToSrgb(rgba[2]), rgba[3]);
    }

    operator const f32v4 &() const;
    operator const f32v3 &() const;

    Color &operator+=(const Color &right);
    Color &operator-=(const Color &right);
    Color &operator*=(const Color &right);
    Color &operator/=(const Color &right);
    template <typename T> Color &operator*=(const T &right)
    {
        rgb = Math::Clamp(rgb * right, 0.f, 1.f);
        return *this;
    }
    template <typename T> Color &operator/=(const T &right)
    {
        rgb = Math::Clamp(rgb / right, 0.f, 1.f);
        return *this;
    }

    friend Color operator+(const Color &left, const Color &right)
    {
        Color result = left;
        result += right;
        return result;
    }

    friend Color operator-(const Color &left, const Color &right)
    {
        Color result = left;
        result -= right;
        return result;
    }

    friend Color operator*(const Color &left, const Color &right)
    {
        Color result = left;
        result *= right;
        return result;
    }

    friend Color operator/(const Color &left, const Color &right)
    {
        Color result = left;
        result /= right;
        return result;
    }

    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator==(const Color &left, const Color &right)
    {
        return left.rgba == right.rgba;
    }
    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator!=(const Color &left, const Color &right)
    {
        return !(left == right);
    }
};

inline Color Color_Red{255u, 0u, 0u};
inline Color Color_Green{0u, 255u, 0u};
inline Color Color_Blue{0u, 0u, 255u};
inline Color Color_Magenta{255u, 0u, 255u};
inline Color Color_Cyan{0u, 255u, 255u};
inline Color Color_Orange{255u, 165u, 0u};
inline Color Color_Yellow{255u, 255u, 0u};
inline Color Color_Black{0u};
inline Color Color_Pink{255u, 192u, 203u};
inline Color Color_Purple{191u, 64u, 191u};
inline Color Color_White{255u};
inline Color Color_Transparent{Color_White, 0u};

class Gradient
{
  public:
    Gradient(TKit::Span<const Color> colors);

    Color Evaluate(f32 t) const;

  private:
    TKit::Span<const Color> m_Colors;
};
#undef CHECK_RGBA
} // namespace Onyx
