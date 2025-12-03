#include "sbox/argparse.hpp"
#include <argparse/argparse.hpp>

namespace Onyx::Demo
{
Dimension ParseArguments(int argc, char **argv)
{
    argparse::ArgumentParser parser{"onyx", ONYX_VERSION, argparse::default_arguments::all};
    parser.add_description(
        "Onyx is a small application framework I have implemented to be used primarily in all projects I develop "
        "that require some sort of rendering. It is built on top of the Vulkan API and provides a simple and "
        "easy-to-use (or so I tried) interface for creating windows, rendering shapes, and handling input events. "
        "The framework is still in its early stages, but I plan to expand it further in the future. This is a small "
        "demo to showcase its features.");

    parser.add_epilog("For similar projects, visit my GitHub at https://github.com/ismawno");

    auto &scene = parser.add_mutually_exclusive_group();
    scene.add_argument("--2-scene").flag().help("Setup a default 2D scene.");
    scene.add_argument("--3-scene").flag().help("Setup a default 3D scene.");

    parser.parse_args(argc, argv);

    return parser.get<bool>("--3-scene") ? D3 : D2;
}
} // namespace Onyx::Demo
