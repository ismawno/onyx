#pragma once

#include "onyx/core/alias.hpp"
#include "onyx/core/glm.hpp"
#include "kit/core/concepts.hpp"

namespace ONYX
{
struct Color
{
    explicit Color(f32 p_Val = 1.f) noexcept;
    explicit Color(u32 p_Val) noexcept;
    explicit Color(u8 p_Val) noexcept;

    explicit(false) Color(const vec4 &p_RGBA) noexcept;
    explicit(false) Color(const vec3 &p_RGB, f32 p_Alpha = 1.f) noexcept;

    Color(f32 p_Red, f32 p_Green, f32 p_Blue, f32 p_Alpha = 1.f) noexcept;
    Color(u32 p_Red, u32 p_Green, u32 p_Blue, u32 p_Alpha = 255) noexcept;
    Color(u8 p_Red, u8 p_Green, u8 p_Blue, u8 p_Alpha = 255) noexcept;

    Color(const Color &p_RGB, f32 p_Alpha) noexcept;
    Color(const Color &p_RGB, u32 p_Alpha) noexcept;
    Color(const Color &p_RGB, u8 p_Alpha) noexcept;

    union {
        vec4 RGBA;
        vec3 RGB;
    };

    u8 Red() const noexcept;
    u8 Green() const noexcept;
    u8 Blue() const noexcept;
    u8 Alpha() const noexcept;

    void Red(u8 p_Red) noexcept;
    void Green(u8 p_Green) noexcept;
    void Blue(u8 p_Blue) noexcept;
    void Alpha(u8 p_Alpha) noexcept;

    template <typename T> T ToHexadecimal(bool p_Alpha = true) const noexcept;

    static Color FromHexadecimal(u32 p_Hex, bool p_Alpha = true) noexcept;
    static Color FromHexadecimal(const std::string &p_Hex, bool p_Alpha = true) noexcept;

    static Color FromString(std::string_view p_Color) noexcept;

    const f32 *Pointer() const noexcept;
    f32 *Pointer() noexcept;

    explicit(false) operator const vec4 &() const noexcept;
    explicit(false) operator const vec3 &() const noexcept;

    Color &operator+=(const Color &p_Right) noexcept;
    Color &operator-=(const Color &p_Right) noexcept;
    Color &operator*=(const Color &p_Right) noexcept;
    Color &operator/=(const Color &p_Right) noexcept;
    template <typename T> Color &operator*=(const T &p_Right) noexcept
    {
        RGB = glm::clamp(RGB * p_Right, 0.f, 1.f);
        return *this;
    }
    template <typename T> Color &operator/=(const T &p_Right) noexcept
    {
        RGB = glm::clamp(RGB / p_Right, 0.f, 1.f);
        return *this;
    }

    friend Color operator+(const Color &p_Left, const Color &p_Right) noexcept
    {
        Color result = p_Left;
        result += p_Right;
        return result;
    }

    friend Color operator-(const Color &p_Left, const Color &p_Right) noexcept
    {
        Color result = p_Left;
        result -= p_Right;
        return result;
    }

    friend Color operator*(const Color &p_Left, const Color &p_Right) noexcept
    {
        Color result = p_Left;
        result *= p_Right;
        return result;
    }

    friend Color operator/(const Color &p_Left, const Color &p_Right) noexcept
    {
        Color result = p_Left;
        result /= p_Right;
        return result;
    }

    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator==(const Color &lhs, const Color &rhs) noexcept
    {
        return lhs.RGBA == rhs.RGBA;
    }
    // Sonarlint yells lol but this is a union like class and no default equality operator is provided
    friend bool operator!=(const Color &lhs, const Color &rhs) noexcept
    {
        return lhs.RGBA != rhs.RGBA;
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
    static const HashMap<std::string, Color> s_ColorMap;
};
} // namespace ONYX