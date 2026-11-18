#!/usr/bin/env python3
"""Stitch the screenshot gallery into docs/images/demo.gif.

The README's demo section shows an animated walk-through; this script
keeps it in sync with the gallery instead of hand-editing a binary.

    python3 scripts/make_demo_gif.py            # default frame order
    python3 scripts/make_demo_gif.py 01 03 05   # custom frame order

Frames come from docs/screenshots/<name>.webp (the PNG the screenshot
driver writes is converted by convert_screenshots.py first).  Output is
an optimized, infinitely looping GIF capped to 900 px wide so it stays
light enough to load instantly on the GitHub landing page.
"""
import sys
from pathlib import Path

from PIL import Image, ImageSequence

REPO = Path(__file__).resolve().parent.parent
SHOTS = REPO / "docs" / "screenshots"
OUT = REPO / "docs" / "images" / "demo.gif"

DEFAULT_ORDER = [
    "01_launcher", "02_login", "03_client", "04_restaurant_list",
    "05_menu", "06_checkout", "07_profile_wallet", "08_tracking",
    "09_admin", "11_client_dark", "12_checkout_dark", "13_admin_server",
]
MAX_WIDTH = 900
FRAME_MS = 1500


def load_frames(order):
    frames = []
    for name in order:
        path = SHOTS / f"{name}.webp"
        if not path.exists():
            print(f"skip (missing): {path.name}")
            continue
        with Image.open(path) as im:
            im.seek(0)  # animated webp sources show their first frame
            frame = im.convert("RGB")
            if frame.width > MAX_WIDTH:
                height = round(frame.height * MAX_WIDTH / frame.width)
                frame = frame.resize((MAX_WIDTH, height), Image.LANCZOS)
            frames.append((name, frame.copy()))
    return frames


def main():
    order = sys.argv[1:] or DEFAULT_ORDER
    frames = load_frames(order)
    if len(frames) < 2:
        raise SystemExit(f"need at least 2 frames, got {len(frames)}")

    first = frames[0][1]
    # Pad every frame onto the first frame's canvas so the loop never jitters.
    normalized = []
    for name, frame in frames:
        canvas = Image.new("RGB", first.size, (24, 24, 28))
        canvas.paste(frame, ((first.width - frame.width) // 2,
                             (first.height - frame.height) // 2))
        normalized.append((name, canvas))

    OUT.parent.mkdir(parents=True, exist_ok=True)
    _, first_frame = normalized[0]
    rest = [f for _, f in normalized[1:]]
    first_frame.save(
        OUT, save_all=True, append_images=rest, duration=FRAME_MS, loop=0,
        optimize=True)
    size_kb = OUT.stat().st_size / 1024
    print(f"demo.gif: {len(normalized)} frames, {size_kb:.0f} KiB -> {OUT}")
    print("sequence:", " -> ".join(name for name, _ in normalized))


if __name__ == "__main__":
    main()
