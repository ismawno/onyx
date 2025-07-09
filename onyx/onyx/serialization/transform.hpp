#pragma once

#include "onyx/core/dimension.hpp"
#include "onyx/property/transform.hpp"
#include "tkit/serialization/yaml/codec.hpp"
#include "tkit/serialization/yaml/glm.hpp"

template <Onyx::Dimension D> struct TKit::Yaml::Codec<Onyx::Transform<D>>
{
    static Node Encode(const Onyx::Transform<D> &p_Transform) noexcept
    {
        Node node;
        node["Translation"] = p_Transform.Translation;
        node["Scale"] = p_Transform.Scale;
        node["Rotation"] = p_Transform.Rotation;
        return node;
    }

    static bool Decode(const Node &p_Node, Onyx::Transform<D> &p_Transform) noexcept
    {
        if (!p_Node.IsMap())
            return false;

        p_Transform.Translation = p_Node["Translation"].as<Onyx::fvec<D>>();
        p_Transform.Scale = p_Node["Scale"].as<Onyx::fvec<D>>();
        p_Transform.Rotation = p_Node["Rotation"].as<Onyx::rot<D>>();

        return true;
    }
};
