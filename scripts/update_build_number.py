#!/usr/bin/env python3
import sys
from pathlib import Path


def main():
    if len(sys.argv) != 3:
        print("Usage: update_build_number.py <number_file> <header_file>", file=sys.stderr)
        return 1
    number_path = Path(sys.argv[1])
    header_path = Path(sys.argv[2])

    if number_path.exists():
        try:
            current = int(number_path.read_text().strip())
        except ValueError:
            current = 0
    else:
        current = 0

    new_value = current + 1

    number_path.write_text(f"{new_value}\n")

    header_path.write_text(
        "#pragma once\n\n"
        f"constexpr int kBuildNumber = {new_value};\n"
    )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
