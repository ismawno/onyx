#pragma once

#include "onyx/property/color.hpp"
#include "tkit/serialization/yaml/codec.hpp"

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
