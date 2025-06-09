#include "utils/argparse.hpp"
#include "onyx/core/core.hpp"

#include <argparse/argparse.hpp>

namespace Onyx::Demo
{
Scene ParseArguments(int argc, char **argv)
{
    argparse::ArgumentParser parser{"drizzle", ONYX_VERSION, argparse::default_arguments::all};
    parser.add_description(
        "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
        "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
        "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
        "The framework is still in its early stages, but I plan to expand it further in the future. This is a small "
        "demo to showcase its features.");

    parser.add_epilog("For similar projects, visit my GitHub at https://github.com/ismawno");

    auto &group = parser.add_mutually_exclusive_group();
    group.add_argument("--2-scene").flag().help("Setup a default 2D scene.");
    group.add_argument("--3-scene").flag().help("Setup a default 3D scene.");

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const std::exception &err)
    {
        std::cerr << err.what() << std::endl;
        std::exit(EXIT_FAILURE);
    }

    if (parser.get<bool>("--2-scene"))
        return Scene::Setup2D;
    if (parser.get<bool>("--3-scene"))
        return Scene::Setup3D;
    return Scene::None;
}
} // namespace Onyx::Demo
