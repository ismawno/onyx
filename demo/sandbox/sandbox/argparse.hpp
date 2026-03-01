#pragma once

#include "onyx/core/alias.hpp"
#include "sandbox/sandbox.hpp"

namespace Onyx
{
using ParseFlags = u8;
enum ParseFlagBit : ParseFlags
{
    ParseFlag_D2 = 1 << 0,
    ParseFlag_D3 = 1 << 1,
    ParseFlag_AddLattice = 1 << 2,
};

struct ParseData
{
    ParseFlags Flags = 0;
    LatticeData<D2> Lattice2{};
    LatticeData<D3> Lattice3{};
};

ParseData ParseArgs(int argc, char **argv);
} // namespace Onyx
