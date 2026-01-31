#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/math.hpp"
#include "tkit/container/span.hpp"

namespace Onyx::Detail
{
template <Numeric T> constexpr T ToType(const f32 val)
{
    if constexpr (Float<T>)
        return val;
    else
        return static_cast<T>(val * 255.f);
}
template <Numeric T> constexpr f32 FromType(const T val)
{
    if constexpr (Float<T>)
        return val;
    else
        return static_cast<T>(val) / 255.f;
}
} // namespace Onyx::Detail

namespace Onyx
{
struct Color
{
    Color(const f32v4 &rgba);
    Color(const f32v3 &rgb, f32 alpha = 1.f);

    Color(f32 val = 1.f);
    Color(u32 val);

    Color(f32 red, f32 green, f32 blue, f32 alpha = 1.f);
    Color(u32 red, u32 green, u32 blue, u32 alpha = 255);

    Color(const Color &rgb, f32 alpha);
    Color(const Color &rgb, u32 alpha);

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

    u32 Pack() const;
    static Color Unpack(u32 packed);

    template <typename T> T ToHexadecimal(bool alpha = true) const;

    static Color FromHexadecimal(u32 hex, bool alpha = true);
    static Color FromHexadecimal(std::string_view hex);

    static Color FromString(const std::string &color);

    const f32 *GetData() const;
    f32 *GetData();

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

    static const Color Red;
    static const Color Green;
    static const Color Blue;
    static const Color Magenta;
    static const Color Cyan;
    static const Color Orange;
    static const Color Yellow;
    static const Color Black;
    static const Color Pink;
    static const Color Purple;
    static const Color White;
    static const Color Transparent;

  private:
    // This will be useful for serialization (not implemented yet)
    static const std::unordered_map<std::string, Color> s_ColorMap;
};

class Gradient
{
  public:
    Gradient(TKit::Span<const Color> colors);

    Color Evaluate(f32 t) const;

  private:
    TKit::Span<const Color> m_Colors;
};
} // namespace Onyx
