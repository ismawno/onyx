#pragma once

#include "onyx/core/api.hpp"
#include "onyx/core/glm.hpp"
#include "tkit/container/span.hpp"

namespace Onyx
{
struct ONYX_API Color
{
    Color(const f32v4 &p_RGBA);
    Color(const f32v3 &p_RGB, f32 p_Alpha = 1.f);

    Color(f32 p_Val = 1.f);
    Color(u32 p_Val);

    Color(f32 p_Red, f32 p_Green, f32 p_Blue, f32 p_Alpha = 1.f);
    Color(u32 p_Red, u32 p_Green, u32 p_Blue, u32 p_Alpha = 255);

    Color(const Color &p_RGB, f32 p_Alpha);
    Color(const Color &p_RGB, u32 p_Alpha);

    union {
        f32v4 RGBA;
        f32v3 RGB;
    };

    u8 Red() const;
    u8 Green() const;
    u8 Blue() const;
    u8 Alpha() const;

    void Red(u32 p_Red);
    void Green(u32 p_Green);
    void Blue(u32 p_Blue);
    void Alpha(u32 p_Alpha);

    u32 Pack() const;
    static Color Unpack(u32 p_Packed);

    template <typename T> T ToHexadecimal(bool p_Alpha = true) const;

    static Color FromHexadecimal(u32 p_Hex, bool p_Alpha = true);
    static Color FromHexadecimal(std::string_view p_Hex);

    static Color FromString(const std::string &p_Color);

    const f32 *GetData() const;
    f32 *GetData();

    operator const f32v4 &() const;
    operator const f32v3 &() const;

    Color &operator+=(const Color &p_Right);
    Color &operator-=(const Color &p_Right);
    Color &operator*=(const Color &p_Right);
    Color &operator/=(const Color &p_Right);
    template <typename T> Color &operator*=(const T &p_Right)
    {
        RGB = Math::Clamp(RGB * p_Right, 0.f, 1.f);
        return *this;
    }
    template <typename T> Color &operator/=(const T &p_Right)
    {
        RGB = Math::Clamp(RGB / p_Right, 0.f, 1.f);
        return *this;
    }

    friend Color operator+(const Color &p_Left, const Color &p_Right)
    {
        Color result = p_Left;
        result += p_Right;
        return result;
    }

    friend Color operator-(const Color &p_Left, const Color &p_Right)
    {
        Color result = p_Left;
        result -= p_Right;
        return result;
    }

    friend Color operator*(const Color &p_Left, const Color &p_Right)
    {
        Color result = p_Left;
        result *= p_Right;
        return result;
    }

    friend Color operator/(const Color &p_Left, const Color &p_Right)
    {
        Color result = p_Left;
        result /= p_Right;
        return result;
    }

    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator==(const Color &p_Left, const Color &p_Right)
    {
        return p_Left.RGBA == p_Right.RGBA;
    }
    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator!=(const Color &p_Left, const Color &p_Right)
    {
        return !(p_Left == p_Right);
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
    static const TKit::HashMap<std::string, Color> s_ColorMap;
};

class ONYX_API Gradient
{
  public:
    Gradient(TKit::Span<const Color> p_Colors);

    Color Evaluate(f32 p_T) const;

  private:
    TKit::Span<const Color> m_Colors;
};

} // namespace Onyx
