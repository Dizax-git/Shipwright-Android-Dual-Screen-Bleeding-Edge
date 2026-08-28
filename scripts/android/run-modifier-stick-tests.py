from __future__ import annotations

import argparse
import os
from pathlib import Path
import shutil
import subprocess


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", type=Path, required=True)
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[2]
    source = root / "scripts/android/tests/native/modifier-stick-c-buttons-test.cpp"
    args.build_dir.mkdir(parents=True, exist_ok=True)
    output = args.build_dir / ("modifier-stick-c-buttons-test.exe" if os.name == "nt" else "modifier-stick-c-buttons-test")

    compiler = next((path for name in ("g++", "clang++", "c++") if (path := shutil.which(name))), None)
    if compiler is None:
        raise RuntimeError("No supported host C++ compiler found (g++, clang++, or c++)")

    subprocess.run(
        [
            compiler,
            "-std=c++20",
            "-Wall",
            "-Wextra",
            "-Werror",
            f"-I{root}",
            str(source),
            "-o",
            str(output),
        ],
        cwd=root,
        check=True,
    )
    subprocess.run([str(output)], cwd=root, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
