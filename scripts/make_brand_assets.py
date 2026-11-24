#!/usr/bin/env python3
"""Generate every branding asset for IUT-Food from one drawing routine.

The brand mark is a white serving cloche on the theme's blue tile
(#0078D7 family, the same blue the Qt stylesheet uses for buttons).
Everything is drawn programmatically (Pillow, 4x supersampled), so the
whole identity is reproducible:

    python3 scripts/make_brand_assets.py

Outputs:
    resources/icons/app-icon.png                     256 px window icon
    packaging/windows/iut-food.ico                   16..256 px exe icon
    packaging/macos/iut-food.icns                    .app bundle icon
    packaging/linux/iut-food.png                     512 px hi-dpi icon
    packaging/linux/iut-food-client.bin.png          256 px deb icon
    packaging/android/res/mipmap-*/ic_launcher*.png  launcher + round
    packaging/android/res/mipmap-xxxhdpi/ic_launcher_foreground.png
"""
from pathlib import Path

from PIL import Image, ImageDraw

REPO = Path(__file__).resolve().parent.parent

# Brand gradient (matches the stylesheet primary #0078d7).
TOP = (42, 164, 240, 255)      # #2aa4f0
MID = (0, 120, 215, 255)       # #0078d7
BOT = (4, 81, 155, 255)        # #04519b
WHITE = (255, 255, 255, 255)
CORNER = 0.225                 # tile corner radius / size

ICO_SIZES = [16, 24, 32, 48, 64, 128, 256]
ICNS_SIZES = [16, 32, 64, 128, 256, 512, 1024]
MIPMAPS = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}


def lerp(a, b, t):
    return tuple(round(a[i] + (b[i] - a[i]) * t) for i in range(4))


def tile_at(big: int) -> Image.Image:
    """The blue rounded-square tile at exactly 'big' pixels."""
    # Vertical gradient built as a 1 x big strip (one colour per row)
    # and stretched horizontally - exact rows, no per-pixel loop.
    col = Image.new("RGBA", (1, big))
    cp = col.load()
    half = big // 2
    for y in range(big):
        cp[0, y] = lerp(TOP, MID, y / half) if y < half else lerp(MID, BOT, (y - half) / half)
    grad = col.resize((big, big))
    mask = Image.new("L", (big, big), 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [0, 0, big - 1, big - 1], radius=round(big * CORNER), fill=255)
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    img.paste(grad, (0, 0), mask)
    return img


def glyph_at(big: int) -> Image.Image:
    """White serving cloche: knob, dome, rim bar and plate."""
    img = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    s = big
    r = round(s * 0.040)
    cx = round(s * 0.500)
    d.ellipse([cx - r, round(s * 0.322) - r, cx + r, round(s * 0.322) + r], fill=WHITE)
    d.pieslice([round(s * 0.260), round(s * 0.352),
                round(s * 0.740), round(s * 0.592)], 180, 360, fill=WHITE)
    d.rounded_rectangle([round(s * 0.232), round(s * 0.594),
                         round(s * 0.768), round(s * 0.622)],
                        radius=round(s * 0.014), fill=WHITE)
    d.rounded_rectangle([round(s * 0.158), round(s * 0.660),
                         round(s * 0.842), round(s * 0.688)],
                        radius=round(s * 0.014), fill=WHITE)
    return img


def icon(size: int) -> Image.Image:
    """Full brand icon: tile + glyph drawn at 4x, downscaled once."""
    big = size * 4
    canvas = tile_at(big)
    canvas.alpha_composite(glyph_at(big))
    return canvas.resize((size, size), Image.LANCZOS)


def write(path: Path, img: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    print(f"  wrote {path.relative_to(REPO)}  ({img.size[0]}x{img.size[1]})")


def main() -> None:
    master = icon(1024)

    # Window icon (Qt resource) + deb icon + hi-dpi Linux icon.
    write(REPO / "resources/icons/app-icon.png", master.resize((256, 256), Image.LANCZOS))
    write(REPO / "packaging/linux/iut-food-client.bin.png",
          master.resize((256, 256), Image.LANCZOS))
    write(REPO / "packaging/linux/iut-food.png", master.resize((512, 512), Image.LANCZOS))

    # Windows Explorer / taskbar icon (multi-resolution .ico).
    ico_path = REPO / "packaging/windows/iut-food.ico"
    ico_path.parent.mkdir(parents=True, exist_ok=True)
    master.save(ico_path, format="ICO",
                sizes=[(s, s) for s in ICO_SIZES],
                append_images=[master.resize((s, s), Image.LANCZOS) for s in ICO_SIZES])
    print(f"  wrote {ico_path.relative_to(REPO)}  ({', '.join(map(str, ICO_SIZES))})")

    # macOS .app bundle icon (multi-resolution .icns).
    icns_path = REPO / "packaging/macos/iut-food.icns"
    icns_path.parent.mkdir(parents=True, exist_ok=True)
    master.save(icns_path, format="ICNS",
                sizes=[(s, s) for s in ICNS_SIZES],
                append_images=[master.resize((s, s), Image.LANCZOS) for s in ICNS_SIZES])
    print(f"  wrote {icns_path.relative_to(REPO)}  ({', '.join(map(str, ICNS_SIZES))})")

    # Android launcher icons (all densities + round + adaptive foreground).
    res = REPO / "packaging/android/res"
    for density, px in MIPMAPS.items():
        img = icon(px)
        write(res / f"mipmap-{density}/ic_launcher.png", img)
        mask = Image.new("L", (px, px), 0)
        ImageDraw.Draw(mask).ellipse([0, 0, px - 1, px - 1], fill=255)
        round_icon = Image.new("RGBA", (px, px), (0, 0, 0, 0))
        round_icon.paste(img, (0, 0), mask)
        write(res / f"mipmap-{density}/ic_launcher_round.png", round_icon)
    # Adaptive foreground: the full tile at 62% inside the 108dp canvas,
    # so every launcher crop keeps the mark inside its safe zone.
    fg = Image.new("RGBA", (432, 432), (0, 0, 0, 0))
    fg.alpha_composite(icon(268), ((432 - 268) // 2, (432 - 268) // 2))
    write(res / "mipmap-xxxhdpi/ic_launcher_foreground.png", fg)

    print("brand assets regenerated")


if __name__ == "__main__":
    main()
