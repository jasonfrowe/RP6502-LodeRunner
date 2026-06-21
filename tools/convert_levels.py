#!/usr/bin/env python3

import argparse
from pathlib import Path
import sys


WIDTH_TILES = 28
HEIGHT_TILES = 16
LEVEL_COUNT = 150

TILE_IDS = {
    "#": 1,
    "$": 2,
    "S": 43,
    "H": 4,
    "0": 44,
    "&": 45,
    "@": 6,
    "-": 5,
    "X": 46,
}


def convert_level(input_path: Path, output_path: Path) -> None:
    with input_path.open("r", encoding="ascii", newline="") as f:
        lines = f.read().splitlines()
    if len(lines) != HEIGHT_TILES:
        raise ValueError(
            f"{input_path}: expected {HEIGHT_TILES} rows, found {len(lines)}"
        )

    data = bytearray()
    for row_index, line in enumerate(lines):
        if len(line) > WIDTH_TILES:
            raise ValueError(
                f"{input_path}: row {row_index + 1} is longer than {WIDTH_TILES} columns"
            )
        padded = line.ljust(WIDTH_TILES, " ")
        for tile_char in padded:
            tile_id = TILE_IDS.get(tile_char, 0)
            data.append(tile_id)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Convert Lode Runner text levels to binary tile maps."
    )
    parser.add_argument(
        "--levels-dir",
        default="loderunner-ng/share/loderunner-ng/levels",
        help="Directory containing text levels named 001..150.",
    )
    parser.add_argument(
        "--out-dir",
        default="maps",
        help="Directory for generated .bin files.",
    )

    args = parser.parse_args()
    levels_dir = Path(args.levels_dir)
    out_dir = Path(args.out_dir)

    for level_num in range(1, LEVEL_COUNT + 1):
        name = f"{level_num:03d}"
        input_path = levels_dir / name
        output_path = out_dir / f"{name}.bin"
        try:
            convert_level(input_path, output_path)
        except FileNotFoundError:
            print(f"Error: missing level file: {input_path}", file=sys.stderr)
            return 1
        except Exception as exc:
            print(f"Error: {exc}", file=sys.stderr)
            return 1

        print(f"Wrote {output_path}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())