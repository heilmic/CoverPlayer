"""Encode native SDL demo frames as a small, seamless README animation."""
from __future__ import annotations
import argparse
from pathlib import Path
from PIL import Image


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("frames", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    paths = sorted(args.frames.glob("frame-*.bmp"))
    if len(paths) != 190:
        raise SystemExit(f"Expected 190 frames, found {len(paths)}")
    # One palette across the animation prevents color flicker between frames.
    samples = Image.new("RGB", (640, 480 * 10))
    for index in range(10):
        with Image.open(paths[index * 20]) as frame:
            if frame.size != (640, 480):
                raise SystemExit(f"Unexpected frame dimensions: {frame.size}")
            samples.paste(frame.convert("RGB"), (0, index * 480))
    palette = samples.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    frames = []
    for path in paths:
        with Image.open(path) as frame:
            frames.append(frame.convert("RGB").quantize(palette=palette, dither=Image.Dither.NONE))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    frames[0].save(args.output, save_all=True, append_images=frames[1:],
                   duration=40, loop=0, optimize=True, disposal=1)
    with Image.open(args.output) as result:
        duration = sum(result.seek(i) or result.info.get("duration", 0) for i in range(result.n_frames))
        if result.size != (640, 480) or duration != 7600 or result.info.get("loop") != 0:
            raise SystemExit("GIF dimensions, duration or loop verification failed")
        print(f"Saved {args.output}: {result.n_frames} frames, {duration / 1000:.1f}s, {args.output.stat().st_size:,} bytes")


if __name__ == "__main__":
    main()
