#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/math.hpp"
#include "tkit/container/span.hpp"

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
        f32v4 RGBA;
        f32v3 RGB;
    };

    u8 Red() const;
    u8 Green() const;
    u8 Blue() const;
    u8 Alpha() const;

    void Red(u32 red);
    void Green(u32 green);
    void Blue(u32 blue);
    void Alpha(u32 alpha);

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
        RGB = Math::Clamp(RGB * right, 0.f, 1.f);
        return *this;
    }
    template <typename T> Color &operator/=(const T &right)
    {
        RGB = Math::Clamp(RGB / right, 0.f, 1.f);
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
        return left.RGBA == right.RGBA;
    }
    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator!=(const Color &left, const Color &right)
    {
        return !(left == right);
    }

    static const Color RED;
    static const Color GREEN;
    static const Color BLUE;
    static const Color MAGENTA;
    static const Color CYAN;
    static const Color ORANGE;
    static const Color YELLOW;
    static const Color BLACK;
    static const Color PINK;
    static const Color PURPLE;
    static const Color WHITE;
    static const Color TRANSPARENT;

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
