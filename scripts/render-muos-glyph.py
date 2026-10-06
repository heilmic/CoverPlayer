"""Render a theme-tintable compact muOS application glyph.

Run with Pillow installed: python scripts/render-muos-glyph.py
The generated PNG is committed so package builds do not need Pillow.
"""

from pathlib import Path

from PIL import Image, ImageDraw


SIZE = 26
SCALE = 8
OUTPUT = Path(__file__).resolve().parents[1] / "packaging/muos/glyph/coverplayer.png"


def xy(values: tuple[float, ...]) -> tuple[int, ...]:
    return tuple(round(value * SCALE) for value in values)


canvas = Image.new("RGBA", (SIZE * SCALE, SIZE * SCALE), (0, 0, 0, 0))
draw = ImageDraw.Draw(canvas)

# Jacaranda recolors app glyphs from their alpha channel. Keep the cover
# interior transparent so the mark does not become a solid theme-colored box.
draw.rounded_rectangle(xy((2, 5, 19, 22)), radius=round(2 * SCALE),
                       outline=(255, 255, 255, 255), width=round(2 * SCALE))
draw.rounded_rectangle(xy((6, 2, 24, 24)), radius=round(2 * SCALE),
                       outline=(255, 255, 255, 255), width=round(2 * SCALE))
draw.polygon([xy((12, 8)), xy((12, 18)), xy((20, 13))],
             fill=(255, 255, 255, 255))

OUTPUT.parent.mkdir(parents=True, exist_ok=True)
canvas.resize((SIZE, SIZE), Image.Resampling.LANCZOS).save(OUTPUT)
