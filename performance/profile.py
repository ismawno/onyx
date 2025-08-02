from pathlib import Path
from argparse import ArgumentParser, Namespace

import datetime as dt
import subprocess
import time

import copy
from ruamel.yaml import YAML
import sys

sys.path.append(str(Path(__file__).parent.parent))

from convoy import Convoy


def setup_arguments() -> Namespace:
    desc = """
    This is a convenience script to be able to profile in bulk the onyx engine. It is supposed to be executed from the project root.
    """
    parser = ArgumentParser(description=desc)

    parser.add_argument("-n", "--name", type=str, default=None, help="An optional name for the run.")
    parser.add_argument(
        "-e",
        "--exec",
        type=Path,
        default=Path("build/performance/onyx-performance"),
        help="The path to the onyx executable.",
    )
    parser.add_argument(
        "-t",
        "--tracy-exec",
        type=Path,
        default=Path("../vendor/tracy/capture/build/tracy-capture"),
        help="The path to the tracy capture executable.",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        default=Path("performance/results"),
        help="The output directory where all traces will be exported.",
    )
    parser.add_argument(
        "-s",
        "--settings",
        type=Path,
        default=Path("performance/profile.yaml"),
        help="The path to the configuration file of the profiling script. It can be the same as the one passed to the onyx executable, with the addition that the key 'Shape' may be a list of shapes.",
    )
    parser.add_argument(
        "-r", "--run-time", type=float, default=3.0, help="The amount of time the program will be run for each shape."
    )

    return parser.parse_args()


args = setup_arguments()
exec: Path = args.exec.resolve()
tracy: Path = args.tracy_exec.resolve()
path: Path = args.settings.resolve()
output: Path = args.output.resolve()

if not exec.is_file():
    Convoy.exit_error(f"The exec path <underline>{exec}</underline> must exist and point to an executable.")
if not tracy.is_file():
    Convoy.exit_error(f"The tracy path <underline>{tracy}</underline> must exist and point to a tracy executable.")
if not path.is_file():
    Convoy.exit_error(f"The settings path <underline>{path}</underline> must exist and point to a yaml file.")


yaml = YAML(pure=True)
with open(path) as f:
    configurations = yaml.load(f)


now = dt.datetime.now(dt.UTC).strftime("%Y-%m-%d--%H-%M-%S")
name = now if args.name is None else f"{now}-{args.name}"

for cname, cfg in configurations.items():
    piv_shapes = []
    ln = 0
    dirpath = output / name / Convoy.to_dyphen_case(cname)

    for lattice in cfg["Lattices"]:
        sp = lattice["Shape"]
        if not isinstance(sp, list):
            sp = [sp]

        if not sp:
            Convoy.exit_error("Shapes must not be empty.")

        if ln == 0:
            ln = len(sp)
        elif ln != len(sp):
            Convoy.exit_error(f"All shape lists must contain the same number of elements: {len(sp)} vs {ln}")
        piv_shapes.append(sp)

    shapes: list[list[str]] = []
    for i in range(ln):
        shapes.append([])
        for s in piv_shapes:
            shapes[-1].append(s[i])

    for slist in shapes:
        settings = copy.deepcopy(cfg)
        for lattice, shp in zip(settings["Lattices"], slist, strict=True):
            lattice["Shape"] = shp

        sufix = "-".join([Convoy.to_dyphen_case(s) for s in slist])
        dirpath.mkdir(parents=True, exist_ok=True)

        sfile = dirpath / f"settings-{sufix}.yaml"
        trace = dirpath / f"trace-{sufix}.tracy"
        Convoy.log(f"Starting trace... Will be exported to <underline>{trace}</underline>.")

        with open(sfile, "w") as f:
            yaml.dump(settings, f)

        targs = [str(tracy), "-o", str(trace)]
        eargs = [str(exec), "-s", str(sfile), "-r", str(args.run_time)]

        Convoy.log(f"Executing tracy-capture with the following command: <bold>{' '.join(targs)}")

        tprocess = subprocess.Popen(targs)
        time.sleep(1.0)

        Convoy.log(f"Executing onyx performance with the following command: <bold>{' '.join(eargs)}")
        eprocess = subprocess.Popen(eargs)

        eprocess.wait()
