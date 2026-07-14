#pragma once

#include "onyx/alias.hpp"
#include "onyx/math.hpp"
#include "tkit/container/span.hpp"
#define TKIT_DEFAULT_STRING_TIER
#include "tkit/container/string.hpp"
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
    return c <= 0.04045f ? c / 12.92f : Math::Power((c + 0.055f) / 1.055f, 2.4f);
}
constexpr f32 ToSrgb(const f32 c)
{
    return c <= 0.0031308f ? c * 12.92f : 1.055f * Math::Power(c, 1.0f / 2.4f) - 0.055f;
}

// TODO(Isma, 02/07/26): If we end up supporting HDR, this will have to go
#ifdef TKIT_ENABLE_ENSURE
inline void CheckRgba(const f32v4 &rgba)
{
    TKIT_ENSURE(rgba[0] <= 1.f && rgba[0] >= 0.f, "[ONYX][COLOR] Red value must be in the range [0, 1], but was {}",
                rgba[0]);
    TKIT_ENSURE(rgba[1] <= 1.f && rgba[1] >= 0.f, "[ONYX][COLOR] Green value must be in the range [0, 1], but was {}",
                rgba[1]);
    TKIT_ENSURE(rgba[2] <= 1.f && rgba[2] >= 0.f, "[ONYX][COLOR] Blue value must be in the range [0, 1], but was {}",
                rgba[2]);
    TKIT_ENSURE(rgba[3] <= 1.f && rgba[3] >= 0.f, "[ONYX][COLOR] Alpha value must be in the range [0, 1], but was {}",
                rgba[3]);
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
    Color(const f32v4 &rgba) : rgba(rgba)
    {
        CHECK_RGBA(rgba);
    }

    Color(const f32v3 &rgb, const f32 alpha = 1.f) : rgba(rgb, alpha)
    {
        CHECK_RGBA(rgba);
    }

    Color(const f32 rgb = 1.f, const f32 alpha = 1.f) : Color(rgb, rgb, rgb, alpha)
    {
    }
    Color(const u32 rgb, const u32 alpha = 255) : Color(rgb, rgb, rgb, alpha)
    {
    }

    Color(const f32 red, const f32 green, const f32 blue, const f32 alpha = 1.f) : rgba(red, green, blue, alpha)
    {
        CHECK_RGBA(rgba);
    }
    Color(const u32 red, const u32 green, const u32 blue, const u32 alpha = 255)
        : rgba(Detail::FromType(red), Detail::FromType(green), Detail::FromType(blue), Detail::FromType(alpha))
    {
        CHECK_RGBA(rgba);
    }

    Color(const Color &rgb, const f32 alpha) : rgba(rgb.rgb, alpha)
    {
        CHECK_RGBA(rgba);
    }

    Color(const Color &rgb, const u32 alpha) : rgba(rgb.rgb, Detail::FromType(alpha))
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

    static f32v4 ToHSV(const f32v4 &rgba);
    static f32v3 ToHSV(const f32v3 &rgb)
    {
        return f32v3{ToHSV(f32v4{rgb, 1.f})};
    }

    f32v4 ToHSV() const
    {
        return ToHSV(rgba);
    }

    static Color FromHSV(const f32v4 &rgba);
    static Color FromHSV(const f32v3 &rgb)
    {
        return FromHSV(f32v4{rgb, 1.f});
    }

    template <typename T> T ToHexadecimal(bool alpha = true) const
    {
        if constexpr (std::is_same_v<T, u32>)
        {
            if (alpha)
                return r() << 24 | g() << 16 | b() << 8 | a();
            return r() << 16 | g() << 8 | b();
        }
        else
        {
            const u32 hex = ToHexadecimal<u32>(alpha);
            if constexpr (std::is_same_v<T, std::string>)
                return alpha ? TKit::Format("{:08X}", hex) : TKit::Format("{:06X}", hex);
            else
                return alpha ? T::Format("{:08X}", hex) : T::Format("{:06X}", hex);
        }
    }

    static Color FromHexadecimal(u32 hex, bool alpha = true);
    static Color FromHexadecimal(TKit::StringView hex);

    static Color FromString(const TKit::String &color);

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

#ifdef ONYX_ENABLE_COLOR_SERIALIZATION
#    include "tkit/serialization/yaml/codec.hpp"

template <> struct TKit::Yaml::Codec<Onyx::Color>
{
    static Node Encode(const Onyx::Color &color)
    {
        return Node{"#" + color.ToHexadecimal<std::string>(color.Alpha() != 255)};
    }

    static bool Decode(const Node &node, Onyx::Color &color)
    {
        if (node.IsScalar())
        {
            const std::string color = node.as<std::string>();
            if (color[0] == '#')
            {
                const std::string hex = color.substr(1);
                color = Onyx::Color::FromHexadecimal(hex);
                return true;
            }
            color = Onyx::Color::FromString(color);
            return true;
        }
        if (node.IsSequence())
        {
            TKIT_ASSERT(node.size() == 3 || node.size() == 4, "[ONYX] Invalid RGB(A) color");
            if (node.size() == 3)
                color = Onyx::Color{node[0].as<f32>(), node[1].as<f32>(), node[2].as<f32>()};
            else if (node.size() == 4)
                color = Onyx::Color{node[0].as<f32>(), node[1].as<f32>(), node[2].as<f32>(), node[3].as<f32>()};
            return true;
        }
        return false;
    }
};
#endif

} // namespace Onyx
