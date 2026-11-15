#!/usr/bin/env python3
"""Convert the screenshot driver's PNG output into the WebP assets the
documentation ships.

The offscreen driver (scripts/screenshot_driver.cpp, target
iut-screenshot-driver) grabs QWidget pixmaps and can only write PNG.
The README gallery uses WebP for its ~10x smaller payloads, so after a
capture run do:

    QT_QPA_PLATFORM=offscreen ./build/iut-screenshot-driver ./shots
    python3 scripts/convert_screenshots.py ./shots docs/screenshots

Requires Pillow (pip install pillow).
"""
import pathlib
import sys

from PIL import Image


def main() -> None:
    src = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else "shots")
    dst = pathlib.Path(sys.argv[2] if len(sys.argv) > 2 else "docs/screenshots")
    if not src.is_dir():
        sys.exit(f"input directory not found: {src}")
    dst.mkdir(parents=True, exist_ok=True)

    converted = 0
    for png in sorted(src.glob("*.png")):
        image = Image.open(png)
        out = dst / f"{png.stem}.webp"
        image.save(out, "WEBP", quality=88, method=6)
        converted += 1
        print(f"{png.name:28s} -> {out.name:28s} {out.stat().st_size // 1024:4d} KiB")
    print(f"converted {converted} screenshots into {dst}")


if __name__ == "__main__":
    main()
