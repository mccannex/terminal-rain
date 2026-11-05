"""Package the license documents distributed beside every platform binary."""

import argparse
from pathlib import Path
import zipfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Path of the license ZIP to create")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    files = ["LICENSE", "THIRD_PARTY_NOTICES.md", "licenses/CC-BY-3.0.txt",
             "licenses/SDL2.txt", "licenses/TerminalVector.txt"]
    # Read all required documents first so missing notices fail before packaging.
    contents = [(name, (root / name).read_bytes()) for name in files]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, content in contents:
            archive.writestr(name, content)


if __name__ == "__main__":
    main()
