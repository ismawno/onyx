#pragma once

#include "perf/lattice.hpp"

namespace Onyx::Perf
{
struct ParseResult
{
    TKit::StaticArray8<Lattice<D2>> Lattices2;
    TKit::StaticArray8<Lattice<D3>> Lattices3;
    Dimension Dim;
    f32 RunTime;
    bool HasRuntime;
};

ParseResult ParseArguments(int argc, char **argv);

} // namespace Onyx::Perf
