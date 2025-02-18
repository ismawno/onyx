#pragma once

#include "tkit/serialization/yaml.hpp"
#include "onyx/draw/color.hpp"

template <> struct TKit::Yaml::Codec<Onyx::Color>
{
    static Node Encode(const Onyx::Color &p_Color) noexcept
    {
        return Node{"#" + p_Color.ToHexadecimal<std::string>(p_Color.Alpha() != 255)};
    }

    static bool Decode(const Node &p_Node, Onyx::Color &p_Color) noexcept
    {
        if (p_Node.IsScalar())
        {
            const std::string color = p_Node.as<std::string>();
            if (color[0] == '#')
            {
                const std::string hex = color.substr(1);
                TKIT_ASSERT(hex.size() == 6 || hex.size() == 8, "[ONYX] Invalid hexadecimal color");
                p_Color = Onyx::Color::FromHexadecimal(hex, hex.size() == 8);
                return true;
            }
            p_Color = Onyx::Color::FromString(color);
            return true;
        }
        if (p_Node.IsSequence())
        {
            TKIT_ASSERT(p_Node.size() == 3 || p_Node.size() == 4, "[ONYX] Invalid RGB(A) color");
            if (p_Node.size() == 3)
                p_Color = Onyx::Color{p_Node[0].as<f32>(), p_Node[1].as<f32>(), p_Node[2].as<f32>()};
            else if (p_Node.size() == 4)
                p_Color =
                    Onyx::Color{p_Node[0].as<f32>(), p_Node[1].as<f32>(), p_Node[2].as<f32>(), p_Node[3].as<f32>()};
            return true;
        }
        return false;
    }
};