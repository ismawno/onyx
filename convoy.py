import sys
import platform
import subprocess
import os
import ctypes
import glob

from time import perf_counter
from pathlib import Path
from typing import NoReturn, TypeVar

T = TypeVar("T")


class _Style:

    RESET = "\033[0m"
    BOLD = "\033[1m"
    DIM = "\033[2m"
    ITALIC = "\033[3m"  # Not widely supported
    UNDERLINE = "\033[4m"
    BLINK = "\033[5m"
    REVERSE = "\033[7m"
    HIDDEN = "\033[8m"
    STRIKETHROUGH = "\033[9m"

    FG_BLACK = "\033[30m"
    FG_RED = "\033[31m"
    FG_GREEN = "\033[32m"
    FG_YELLOW = "\033[33m"
    FG_BLUE = "\033[34m"
    FG_MAGENTA = "\033[35m"
    FG_CYAN = "\033[36m"
    FG_WHITE = "\033[37m"
    FG_DEFAULT = "\033[39m"

    FG_BRIGHT_BLACK = "\033[90m"
    FG_BRIGHT_RED = "\033[91m"
    FG_BRIGHT_GREEN = "\033[92m"
    FG_BRIGHT_YELLOW = "\033[93m"
    FG_BRIGHT_BLUE = "\033[94m"
    FG_BRIGHT_MAGENTA = "\033[95m"
    FG_BRIGHT_CYAN = "\033[96m"
    FG_BRIGHT_WHITE = "\033[97m"

    BG_BLACK = "\033[40m"
    BG_RED = "\033[41m"
    BG_GREEN = "\033[42m"
    BG_YELLOW = "\033[43m"
    BG_BLUE = "\033[44m"
    BG_MAGENTA = "\033[45m"
    BG_CYAN = "\033[46m"
    BG_WHITE = "\033[47m"
    BG_DEFAULT = "\033[49m"

    BG_BRIGHT_BLACK = "\033[100m"
    BG_BRIGHT_RED = "\033[101m"
    BG_BRIGHT_GREEN = "\033[102m"
    BG_BRIGHT_YELLOW = "\033[103m"
    BG_BRIGHT_BLUE = "\033[104m"
    BG_BRIGHT_MAGENTA = "\033[105m"
    BG_BRIGHT_CYAN = "\033[106m"
    BG_BRIGHT_WHITE = "\033[107m"

    @staticmethod
    def format(text: str, /, *, void: bool = False) -> str:
        translator = _Style.__create_format_dict()
        styles = set()

        splitted = text.split("<")
        formatted = splitted[0]
        for segment in splitted[1:]:
            for key, value in translator.items():
                if segment.startswith(key):
                    styles.add(value)
                    formatted += segment.replace(key, value if not void else "", 1)
                    break
                if segment.startswith(f"/{key}"):
                    styles.remove(value)
                    formatted += segment.replace(
                        f"/{key}",
                        f"{_Style.RESET}{''.join(styles)}" if not void else "",
                        1,
                    )
                    break
            else:
                formatted += f"<{segment}"
        if void:
            return formatted

        return formatted + _Style.RESET

    @staticmethod
    def __create_format_dict() -> dict[str, str]:
        result = {}
        for name, value in _Style.__dict__.items():
            if callable(value) or not isinstance(value, str) or name.startswith("_"):
                continue
            prefix = ""
            if name.startswith("FG_"):
                prefix += "f"
            elif name.startswith("BG_"):
                prefix += "b"
            if "BRIGHT" in name:
                prefix += "b"
            name = name.removeprefix("FG_").removeprefix("BG_").removeprefix("BRIGHT_").lower()

            result[f"{prefix}{name}>"] = value
        return result


class _MetaConvoy(type):
    def __init__(self, *args, **kwargs) -> None:
        self.__t1 = perf_counter()
        super().__init__(*args, **kwargs)
        self.is_verbose = False
        self.all_yes = False
        self.safe = False

        self.__indent = 1

        labels = [
            ("CONVOY", "fblue"),
            ("VERBOSE", "fmagenta"),
            ("LOG", "fgreen"),
            ("WARNING", "fyellow"),
            ("ERROR", "fred"),
            ("FAILURE", "fred"),
            ("SUCCESS", "fgreen"),
            ("PROMPT", "fcyan"),
        ]
        self.__labels: dict[str, str] = {}
        label_indent = 0
        for name, color in labels:
            label = self.__create_label(name, color)
            self.__labels[name.lower()] = label
            lp = self.__create_label_prefix(label)
            ln = len(_Style.format(lp, void=True))
            if label != "CONVOY" and ln > label_indent:
                label_indent = ln

        self.__label_indent: dict[str, int] = {}
        for name, color in labels:
            if name == "CONVOY":
                continue
            label = self.__labels[name.lower()]
            lp = self.__create_label_prefix(label)
            ln = len(_Style.format(lp, void=True))
            self.__label_indent[name.lower()] = label_indent - ln

        self.__no_colors = False
        if self.is_windows:
            kernel32 = ctypes.windll.kernel32
            hstdout = kernel32.GetStdHandle(-11)
            mode = ctypes.c_ulong()
            if not kernel32.GetConsoleMode(hstdout, ctypes.byref(mode)):
                self.__no_colors = True
                self.log("Failed to get console mode. Text colors will be disabled.")

            new_mode = mode.value | 0x0004
            if not kernel32.SetConsoleMode(hstdout, new_mode):
                self.__no_colors = True
                self.log("Failed to set console mode. Text colors will be disabled.")

    @property
    def version(self) -> str:
        return "v0.1.1"

    @property
    def is_windows(self) -> bool:
        return sys.platform.startswith("win")

    @property
    def is_linux(self) -> bool:
        return sys.platform.startswith("linux")

    @property
    def is_macos(self) -> bool:
        return sys.platform.startswith("darwin")

    @property
    def is_arm(self) -> bool:
        return platform.machine().lower() in ["arm64", "aarch64"]

    @property
    def operating_system(self) -> str:
        if self.is_windows:
            return "Windows"
        if self.is_linux:
            return "Linux"
        if self.is_macos:
            return "MacOS"
        return sys.platform

    @property
    def architecure(self) -> str:
        similars = {"i386": "x86", "amd64": "x86_64", "x32": "x86", "x64": "x86_64"}
        try:
            return similars[platform.machine().lower()]
        except KeyError:
            return platform.machine().lower()

    @property
    def program_label(self) -> str:
        return self.__labels["convoy"]

    @program_label.setter
    def program_label(self, msg: str, /) -> None:
        self.__labels["convoy"] = self.__create_label(msg, "fblue") if msg else ""

    @property
    def is_admin(self) -> bool:
        if self.is_windows:
            return ctypes.windll.shell32.IsUserAnAdmin() != 0
        return os.geteuid() == 0

    def linux_distro(self) -> str | None:
        try:
            with open("/etc/os-release") as f:
                for line in f:
                    if line.startswith("ID="):
                        return line.split("=")[1].strip().strip('"')
        except FileNotFoundError:
            return None
        return None

    def linux_version(self) -> str | None:
        try:
            with open("/etc/os-release") as f:
                for line in f:
                    if line.startswith("VERSION_ID="):
                        return line.split("=")[1].strip().strip('"')
        except FileNotFoundError:
            return None
        return None

    def push_indent(self) -> None:
        self.__indent += 1

    def pop_indent(self) -> None:
        self.__indent -= 1

    def verbose(self, msg: str, /, *args, **kwargs) -> None:
        if self.is_verbose:
            self.__print(msg, "verbose", *args, **kwargs)

    def log(self, msg: str, /, *args, **kwargs) -> None:
        self.__print(msg, "log", *args, **kwargs)

    def warning(self, msg: str, /, *args, **kwargs) -> None:
        self.__print(msg, "warning", *args, **kwargs)

    def error(self, msg: str, /, *args, **kwargs) -> None:
        self.__print(msg, "error", file=sys.stderr, *args, **kwargs)

    def exit_ok(self, msg: str | None = None, /) -> NoReturn:
        if msg is not None:
            self.__print(msg, "success")

        self.__exit(0, "success")

    def exit_error(
        self,
        msg: str = "Something went wrong! Likely because something happened or the user declined a prompt.",
        /,
    ) -> NoReturn:
        self.error(msg)
        self.__exit(1, "failure")

    def exit_declined(self) -> NoReturn:
        self.exit_error("Operation declined by user.")

    def exit_restart(self) -> NoReturn:
        self.exit_ok("<bold>RE-RUN REQUIRED</bold>.")

    def to_snake_case(self, s: str, /) -> str:
        result = []
        for c in s:
            if c.isupper():
                result.append(f"_{c.lower()}" if result else c.lower())
            elif c == "-":
                result.append("_")
            else:
                result.append(c)
        return "".join(result)

    def to_camel_case(self, s: str, /) -> str:
        result = []
        s = s[:1].lower() + s[1:]
        for c in s:
            if c == "_" or c == "-":
                result.append(c.upper())
            else:
                result.append(c)
        return "".join(result)

    def to_pascal_case(self, s: str, /) -> str:
        result = []
        s = s.capitalize()
        for c in s:
            if c == "_" or c == "-":
                result.append(c.upper())
            else:
                result.append(c)
        return "".join(result)

    def to_dyphen_case(self, s: str, /) -> str:
        result = []
        for c in s:
            if c.isupper():
                result.append(f"-{c.lower()}" if result else c.lower())
            elif c == "_":
                result.append("-")
            else:
                result.append(c)
        return "".join(result)

    def is_file(self, p: Path, /) -> bool:
        return bool(p.is_file() or p.suffix)

    def is_dir(self, p: Path, /) -> bool:
        return bool(p.is_dir() or not p.suffix)

    def resolve_paths(
        self,
        paths: str | Path | list[str | Path],
        /,
        *,
        cwd: Path | None = None,
        recursive: bool = False,
        check_exists: bool = False,
        require_files: bool = False,
        require_directories: bool = False,
        exclude_files: bool = False,
        exclude_directories: bool = False,
        remove_duplicates: bool = False,
        mkdir: bool = False,
    ) -> list[Path]:
        if require_files and require_directories:
            Convoy.exit_error(
                "Cannot set both <bold>require_files</bold> and <bold>require_directories</bold> options."
            )

        if exclude_files and exclude_directories:
            Convoy.exit_error(
                "Cannot set both <bold>exclude_files</bold> and <bold>exclude_directories</bold> options."
            )

        if not isinstance(paths, list):
            paths = [paths]

        if cwd is None:
            cwd = Path(os.getcwd()).resolve()

        def run_checks(path: Path, /) -> Path | None:
            if check_exists and not path.exists():
                Convoy.exit_error(f"The path <underline>{path}</underline> does not exist.")
            if require_files and not self.is_file(path):
                Convoy.exit_error(f"The path <underline>{path}</underline> is not a file.")
            if require_directories and not self.is_dir(path):
                Convoy.exit_error(f"The path <underline>{path}</underline> is not a directory.")

            if (exclude_files and not self.is_file(path)) or (exclude_directories and not self.is_dir(path)):
                return None

            if mkdir and not self.is_file(path):
                path.mkdir(parents=True, exist_ok=True)
            return path

        result: list[Path] = []
        for path in paths:
            if isinstance(path, Path):
                path = str(path)
            if glob.has_magic(path):
                iterator = cwd.rglob(path) if recursive else cwd.glob(path)
                for p in iterator:
                    p = run_checks(p.resolve())
                    if p is not None:
                        result.append(p)
            else:
                path = run_checks(Path(path).resolve())
                if path is not None:
                    result.append(path)

        return result if not remove_duplicates else list(set(result))

    def ncheck(self, param: T | None, /, *, msg: str | None = None) -> T:
        if param is None:
            self.exit_error(msg or "Found a <bold>None</bold> value.")
        return param

    def prompt(self, msg: str, /, *, default: bool = True) -> bool:
        if self.all_yes:
            return True

        msg = f"{msg} <bold>[Y]</bold>/N " if default else f"{msg} <bold>Y</bold>/[N] "
        msg = self.__format_message(msg, "prompt")

        while True:
            answer = input(msg).strip().lower()
            if answer in ["y", "yes"] or (default and answer == ""):
                return True
            elif answer in ["n", "no"] or (not default and answer == ""):
                return False

    def empty_prompt(self, msg: str, /) -> None:
        msg = self.__format_message(msg, "prompt")
        input(msg)

    def run_process(
        self, command: str | list[str], /, *args, exit_on_decline: bool = True, **kwargs
    ) -> subprocess.CompletedProcess | None:
        if self.safe and not self.prompt(
            f"The command <bold>{command if isinstance(command, str) else ' '.join(command)}</bold> is about to be executed. Do you wish to continue?"
        ):
            if exit_on_decline:
                self.exit_declined()
            return None

        return subprocess.run(command, *args, **kwargs)

    def run_process_success(self, command: str | list[str], /, *args, **kwargs) -> bool:
        result = self.run_process(command, *args, **kwargs, exit_on_decline=False)
        return result is not None and result.returncode == 0

    def run_file(self, path: Path | str, /) -> None:
        if self.safe and not self.prompt(
            f"The file at <underline>{path}</underline> is about to be executed. Do you wish to continue?"
        ):
            self.exit_declined()

        if isinstance(path, Path):
            path = str(path.resolve())
        if self.is_windows:
            os.startfile(path)
        elif self.is_linux:
            self.run_process(["xdg-open", path])
        elif self.is_macos:
            self.run_process(["open", path])

    def nested_split(
        self,
        string: str,
        /,
        delim: str,
        *,
        openers: str | list[str],
        closers: str | list[str],
        n: int | None = None,
    ) -> list[str]:
        if isinstance(openers, str):
            openers = [openers]
        if isinstance(closers, str):
            closers = [closers]
        if len(openers) != len(closers):
            Convoy.exit_error(
                f"Openers and closers length mismatch! Openers: <bold>{', '.join(openers)}</bold>, closers: <bold>{', '.join(closers)}</bold>."
            )
        if not openers or not closers:
            Convoy.exit_error("Both openers and closers must not be empty.")

        matchers = {op: (cl, 0) for op, cl in zip(openers, closers)}

        result = []
        current = []
        remaining = string
        index = 0
        while index < len(string):
            substr = string[index:]
            if n is not None and len(result) == n:
                result.append(substr)
                return result
            tdepth = 0
            for op, (cl, depth) in matchers.items():
                if substr.startswith(op):
                    depth = depth + 1 if op != cl or depth == 0 else depth - 1
                elif substr.startswith(cl):
                    depth -= 1

                matchers[op] = (cl, depth)
                tdepth += depth

            if tdepth == 0 and substr.startswith(delim):
                index += len(delim)
                result.append("".join(current))
                remaining = string[index:]
                current = []
                continue

            current.append(string[index])
            index += 1
        result.append(remaining)
        return result

    def __print(self, msg: str, label: str, /, *args, **kwargs) -> None:
        print(
            self.__format_message(msg, label),
            *args,
            **kwargs,
        )

    def __create_label_prefix(self, label: str, /) -> str:
        plabel = self.__labels["convoy"]
        return f"{plabel}{label}"

    def __format_message(self, msg: str, label: str, /) -> str:
        indent = self.__indent + self.__label_indent[label]
        label = self.__labels[label]
        prefix = self.__create_label_prefix(label)
        return self.__format_ansi(f"{prefix}{' ' * indent}{msg}")

    def __format_ansi(self, text: str, /) -> str:
        return _Style.format(text, void=self.__no_colors)

    def __exit(self, code: int, label: str, /) -> NoReturn:
        elapsed = perf_counter() - self.__t1
        self.__print(f"Finished in <bold>{elapsed:.2f}</bold> seconds.", label)
        sys.exit(code)

    def __create_label(self, name: str, color: str, /) -> str:
        return f"<{color}>[{name}]</{color}> "


class Convoy(metaclass=_MetaConvoy):
    pass
