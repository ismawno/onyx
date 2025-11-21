#include "perf/argparse.hpp"
#include "onyx/core/core.hpp"
#include "onyx/serialization/color.hpp"
#include "tkit/serialization/yaml/tkit/utils/dimension.hpp"
#include "tkit/serialization/yaml/container.hpp"
#include "tkit/serialization/yaml/tensor.hpp"
#include "tkit/serialization/yaml/quaternion.hpp"
#include "tkit/serialization/yaml/onyx/object/primitives.hpp"
#include "tkit/serialization/yaml/onyx/rendering/camera.hpp"
#include "tkit/serialization/yaml/onyx/data/options.hpp"
#include "tkit/serialization/yaml/onyx/property/transform.hpp"
#include "tkit/serialization/yaml/perf/lattice.hpp"

#include <argparse/argparse.hpp>

namespace Onyx::Perf
{
template <Dimension D> void exportLatticeToFile(const Lattice<D> &p_Lattice, ParseResult &result)
{
    TKit::Yaml::Node node;
    node["Dimension"] = D;
    node["Lattices"].push_back(p_Lattice);
    if constexpr (D == D2)
    {
        TKit::Yaml::ToFile(ONYX_ROOT_PATH "/performance/settings-2D.yaml", node);
        result.Lattices2.Append(p_Lattice);
    }
    else
    {
        TKit::Yaml::ToFile(ONYX_ROOT_PATH "/performance/settings-3D.yaml", node);
        result.Lattices3.Append(p_Lattice);
    }
}
ParseResult ParseArguments(int argc, char **argv)
{
    argparse::ArgumentParser parser{"onyx", ONYX_VERSION, argparse::default_arguments::all};
    // parser.add_description(
    //     "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
    //     "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
    //     "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
    //     "The framework is still in its early stages, but I plan to expand it further in the future. This is a small "
    //     "demo to showcase its features.");

    parser.add_description("This is a small performance playground to stress test the Onyx engine. The main method of "
                           "testing the performance is by creating various lattices of objects to be rendered.");

    parser.add_epilog("For similar projects, visit my GitHub at https://github.com/ismawno");

    auto &group1 = parser.add_mutually_exclusive_group(true);

    group1.add_argument("-s", "--settings").help("A path pointing to a yaml file with lattice settings.");
    group1.add_argument("-e", "--export")
        .flag()
        .help("Export a file with a basic lattice configuration so that you can expand it from there.");

    auto &group2 = parser.add_mutually_exclusive_group();
    group2.add_argument("--2-dim").flag().help(
        "In case the --export option is set, choose to run the 2D default lattice. "
        "Will be ignored if --export is not set.");
    group2.add_argument("--3-dim").flag().help(
        "In case the --export option is set, choose to run the 3D default lattice. "
        "Will be ignored if --export is not set.");

    parser.add_argument("-r", "--run-time")
        .scan<'f', f32>()
        .help("The amount of time the program will run for in seconds. If not "
              "specified, the simulation will run indefinitely.");

    parser.parse_args(argc, argv);

    ParseResult result{};

    if (const auto path = parser.present("--settings"))
    {
        const auto settings = TKit::Yaml::FromFile(*path);
        result.Dim = settings["Dimension"].as<Dimension>();
        if (result.Dim == D2)
            for (const TKit::Yaml::Node &node : settings["Lattices"])
                result.Lattices2.Append(node.as<Lattice<D2>>());
        else
            for (const TKit::Yaml::Node &node : settings["Lattices"])
                result.Lattices3.Append(node.as<Lattice<D3>>());
    }
    else if (parser.get<bool>("--export"))
    {
        if (!parser.get<bool>("--2-dim") && !parser.get<bool>("--3-dim"))
        {
            std::cerr << "A dimension must be specified when using --export.\n";
            std::exit(EXIT_FAILURE);
        }
        result.Dim = parser.get<bool>("--2-dim") ? D2 : D3;

        Lattice<D2> l2{};
        Lattice<D3> l3{};

        exportLatticeToFile(l2, result);
        exportLatticeToFile(l3, result);
    }

    if (const auto rt = parser.present<f32>("--run-time"))
    {
        result.RunTime = *rt;
        result.HasRuntime = true;
    }
    else
        result.HasRuntime = false;

    return result;
}
} // namespace Onyx::Perf
