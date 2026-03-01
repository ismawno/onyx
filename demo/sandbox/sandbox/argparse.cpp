#include "sandbox/argparse.hpp"
#include "tkit/serialization/yaml/onyx/property/transform.hpp"
#include "tkit/serialization/yaml/onyx/property/instance.hpp"
#include "tkit/serialization/yaml/sandbox/sandbox.hpp"
#include "tkit/serialization/yaml/container.hpp"
#include "tkit/serialization/yaml/tensor.hpp"
#include "tkit/serialization/yaml/quaternion.hpp"
#include <argparse/argparse.hpp>

namespace Onyx
{
template <Dimension D> void exportLatticeToFile(const LatticeData<D> &lattice)
{
    TKit::Yaml::Node node;
    node["Dimension"] = D;
    node["Lattice"] = lattice;
    if constexpr (D == D2)
        TKit::Yaml::ToFile(ONYX_ROOT_PATH "/demo/sandbox/lattice-2D.yaml", node);
    else
        TKit::Yaml::ToFile(ONYX_ROOT_PATH "/demo/sandbox/lattice-3D.yaml", node);
}

ParseData ParseArgs(int argc, char **argv)
{
    argparse::ArgumentParser parser{"onyx", ONYX_VERSION, argparse::default_arguments::all};
    parser.add_description(
        "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
        "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
        "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
        "The framework is still in its early stages, but I plan to expand it further in the future. This is a small "
        "demo to showcase its features.");

    parser.add_epilog("For similar projects, visit my GitHub at https://github.com/ismawno");

    auto &group = parser.add_mutually_exclusive_group();
    group.add_argument("--2-dim").flag().help("Setup only default 2D scene");
    group.add_argument("--3-dim").flag().help("Setup only default 3D scene");
    group.add_argument("-e", "--export").flag().help("Export a default lattice file for each dimension");
    group.add_argument("-l", "--lattice").help("Path to a lattice yaml file");

    parser.parse_args(argc, argv);

    ParseData data{};
    if (parser.get<bool>("--export"))
    {
        exportLatticeToFile(data.Lattice2);
        exportLatticeToFile(data.Lattice3);
    }
    else if (const auto path = parser.present("--lattice"))
    {
        const auto settings = TKit::Yaml::FromFile(*path);
        const Dimension dim = settings["Dimension"].as<Dimension>();
        if (dim == D2)
        {
            data.Lattice2 = settings["Lattice"].as<LatticeData<D2>>();
            data.Flags |= ParseFlag_D2 | ParseFlag_AddLattice;
        }
        else
        {
            data.Lattice3 = settings["Lattice"].as<LatticeData<D3>>();
            data.Flags |= ParseFlag_D3 | ParseFlag_AddLattice;
        }
    }
    else if (!parser.get<bool>("--2-dim") && !parser.get<bool>("--3-dim"))
        data.Flags |= ParseFlag_D2 | ParseFlag_D3;
    else if (parser.get<bool>("--2-dim"))
        data.Flags |= ParseFlag_D2;
    else
        data.Flags |= ParseFlag_D3;
    return data;
}
} // namespace Onyx
