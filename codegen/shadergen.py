from pathlib import Path
from argparse import ArgumentParser, Namespace
from generator import CPPGenerator
from collections.abc import Callable

import sys
import shutil
import os
import subprocess

sys.path.append(str(Path(__file__).parent.parent))

from convoy import Convoy


def parse_arguments() -> Namespace:
    desc = """
    The purpose of this script is to generate shader spirv code of the onyx shaders.
    """

    parser = ArgumentParser(description=desc)
    parser.add_argument(
        "-i",
        "--input",
        type=Path,
        default=Path.cwd() / "onyx" / "shaders",
        help="The input directory where the shaders are located.",
    )
    parser.add_argument("-e", "--embed", action="store_true", default=False, help="Embed shaders")
    parser.add_argument("-g", "--generate", action="store_true", default=False, help="Generate shaders")
    parser.add_argument("-c", "--compile", action="store_true", default=False, help="Compile shaders")
    parser.add_argument(
        "--embed-path",
        type=Path,
        default=Path.cwd() / "onyx" / "source" / "spirv.hpp",
        help="The file were the embedded code will be generated.",
    )

    parser.add_argument("-p", "--profile", type=str, default="spirv_1_5", help="The spirv profile to compile with")
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        default=False,
        help="Print more information.",
    )

    return parser.parse_args()


Convoy.program_label = "SHADERS"
args = parse_arguments()

ipath: Path = args.input
gpath: Path = ipath / "gen"
cpath: Path = ipath / "bin"
epath: Path = args.embed_path

generate: bool = args.generate
compile: bool = args.compile
embed: bool = args.embed

if (compile or generate) and shutil.which("slangc") is None:
    Convoy.exit_error("The slangc binary must be accessible to generate the spirv code.")

if embed and shutil.which("xxd") is None:
    Convoy.exit_error("The xxd binary must be accessible to embed the spirv code.")

if not ipath.exists() or not ipath.is_dir():
    Convoy.exit_error(f"The input path <underline>{ipath}</underline> must exist and be a directory.")

if generate:
    gpath.mkdir(parents=True, exist_ok=True)
if compile:
    cpath.mkdir(parents=True, exist_ok=True)

dims = ["2D", "3D"]
geos = ["circle", "static", "parametric", "glyph"]
passes = ["flat", "shaded", "shadow"]

fshaders = ["blend", "compositor", "post-process"]
vshaders = ["full-vertex"]
cshaders = ["ray-march"]
standalone = fshaders + vshaders + cshaders

processes: list[tuple[subprocess.Popen, str]] = []
processes: list[tuple[subprocess.Popen, str]] = []
eactions: list[Callable] = []


def process_shader(name: str, entry: str, output: str | None = None, /) -> None:
    if output is None:
        output = name

    iname = f"{name}.slang"
    cname = f"{output}.spv"

    finput = ipath / iname
    coutput = cpath / cname
    if compile:
        Convoy.log(f"Compiling <bold>{iname}</bold> - <bold>{entry}</bold>")
        process = subprocess.Popen(
            [
                "slangc",
                str(finput),
                "-profile",
                args.profile,
                "-matrix-layout-row-major",
                # we need -emit-spirv-via-glsl bc slangc emits Int8 capability even tho we dont use it. because of that, we then need to suppress a warning that slangc emits because it needs to add a bunch of other capabilities
                "-emit-spirv-via-glsl",
                "-Wno-41012",
                "-target",
                "spirv",
                "-o",
                str(coutput),
                "-entry",
                entry,
            ]
        )
        processes.append((process, f"Failed to compile <bold>{iname}</bold>"))
    if embed:

        def action() -> None:
            oldc = os.getcwd()
            os.chdir(cpath)

            Convoy.log(f"Embedding <bold>{cname}</bold>")
            res = Convoy.run_process(["xxd", "-i", cname], text=True, capture_output=True)
            if res is None or res.returncode != 0:
                Convoy.exit_error(f"Failed to embed <bold>{cname}</bold>")
            hpp(res.stdout.replace("unsigned int", "constexpr u32").replace("unsigned char", "constexpr u8"))
            os.chdir(oldc)

        eactions.append(action)


def process_geometry_shader(name: str, /, *, has_transparency: bool) -> None:
    if generate:
        ffile = f"{name}.slang"
        ginput = ipath / ffile
        goutput = gpath / ffile
        Convoy.log(f"Generating <bold>{ffile}</bold>")
        process = subprocess.Popen(
            f"slangc -E {ginput} -Wno-41012 | clang-format --assume-filename=shader.cs --style='{{BasedOnStyle: Microsoft}}' > {goutput}",
            shell=True,
        )
        processes.append((process, f"Failed to generate <bold>{ffile}</bold>."))

    process_shader(name, "mainVS", f"{name}-vert")
    process_shader(name, "mainFSO", f"{name}-frag-opaque")
    if has_transparency:
        process_shader(name, "mainFST", f"{name}-frag-transparent")


for dim in dims:
    for geo in geos:
        for rpass in passes:
            name = f"{geo}-{rpass}-{dim}"
            process_geometry_shader(name, has_transparency=rpass != "shadow")

for f in fshaders:
    process_shader(f, "mainFS")
for v in vshaders:
    process_shader(v, "mainVS")
for c in cshaders:
    process_shader(c, "main")

hpp = CPPGenerator()
hpp.disclaimer("spirv.hpp")
hpp("#pragma once")
hpp.include("onyx/instance.hpp", quotes=True)
hpp.include("pass.hpp", quotes=True)
hpp.include("tkit/container/fixed_array.hpp", quotes=True)

with hpp.scope("namespace Onyx", indent=0):

    for p, msg in processes:
        if p.wait() != 0:
            Convoy.exit_error(msg)
    for ac in eactions:
        ac()

    with hpp.scope("struct ShaderBinary", closer="};"):
        hpp("const u8 *Code;")
        hpp("u32 Count;")

    with hpp.scope("struct ShaderBinaryData", closer="};"):
        hpp(
            "TKit::FixedArray<TKit::FixedArray<TKit::FixedArray<ShaderBinary, Geometry_Count>, RenderPass_Count>, D_Count> VertexShaders;"
            "TKit::FixedArray<TKit::FixedArray<TKit::FixedArray<ShaderBinary, Geometry_Count>, RenderPass_Count>, D_Count> OpaqueFragmentShaders;"
            "TKit::FixedArray<TKit::FixedArray<TKit::FixedArray<ShaderBinary, Geometry_Count>, RenderPass_Count>, D_Count> TransparentFragmentShaders;"
        )
        for shader in standalone:
            hpp(f"ShaderBinary {Convoy.to_pascal_case(shader)};")

    hpp("inline ShaderBinaryData g_ShaderBinaryData;")

    with hpp.scope("inline void InitializeBinaries()"):

        for dim in dims:
            for geo in geos:
                for rpass in passes:
                    name = f"{geo}_{rpass}_{dim}"
                    fopaque = f"{name}_frag_opaque_spv"
                    ftransparent = f"{name}_frag_opaque_spv"
                    vshader = f"{name}_vert_spv"

                    d = "D2" if dim == "2D" else "D3"
                    g = f"Geometry_{geo.capitalize()}"
                    r = f"RenderPass_{rpass.capitalize()}"
                    hpp(f"g_ShaderBinaryData.VertexShaders[{d} - 2][{r}][{g}].Code = {vshader};")
                    hpp(f"g_ShaderBinaryData.VertexShaders[{d} - 2][{r}][{g}].Count = {vshader}_len;")

                    hpp(f"g_ShaderBinaryData.OpaqueFragmentShaders[{d} - 2][{r}][{g}].Code = {fopaque};")
                    hpp(f"g_ShaderBinaryData.OpaqueFragmentShaders[{d} - 2][{r}][{g}].Count = {fopaque}_len;")

                    if rpass != "shadow":
                        hpp(f"g_ShaderBinaryData.TransparentFragmentShaders[{d} - 2][{r}][{g}].Code = {ftransparent};")
                        hpp(
                            f"g_ShaderBinaryData.TransparentFragmentShaders[{d} - 2][{r}][{g}].Count = {ftransparent}_len;"
                        )

        for shader in standalone:
            name = Convoy.to_pascal_case(shader)
            codename = f"{Convoy.to_snake_case(shader)}_spv"
            hpp(f"g_ShaderBinaryData.{name}.Code = {codename};")
            hpp(f"g_ShaderBinaryData.{name}.Count = {codename}_len;")


if embed:
    hpp.write(epath, format=False)

Convoy.exit_ok()
