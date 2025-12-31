#pragma once

#include "perf/lattice.hpp"

namespace Onyx::Demo
{
struct ParseResult
{
    Lattice<D2> Lattice2{};
    Lattice<D3> Lattice3{};
    ShapeSettings Settings{};
    Dimension Dim;
    f32 RunTime;
    bool HasRuntime;
};

ParseResult ParseArguments(int argc, char **argv);

} // namespace Onyx::Demo
