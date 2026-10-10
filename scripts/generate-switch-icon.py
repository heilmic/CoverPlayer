"""Generate the original CoverPlayer hbmenu icon (requires Pillow)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
image = Image.new("RGB", (768, 768), (14, 20, 29))
draw = ImageDraw.Draw(image)
draw.rounded_rectangle((218, 90, 550, 564), radius=48, fill=(92, 211, 151))
draw.rounded_rectangle((251, 128, 517, 356), radius=18, fill=(19, 34, 43))
draw.polygon(((347, 179), (347, 306), (443, 242)), fill=(238, 248, 242))
draw.ellipse((314, 392, 454, 532), fill=(19, 34, 43))
draw.ellipse((354, 432, 414, 492), fill=(238, 248, 242))
font = ImageFont.truetype(str(root / "assets/fonts/RobotoMono-Bold.ttf"), 74)
draw.text((384, 616), "CoverPlayer", font=font, anchor="mt", fill=(238, 248, 242))
target = root / "packaging/switch/icon.jpg"
target.parent.mkdir(parents=True, exist_ok=True)
image.resize((256, 256), Image.Resampling.LANCZOS).save(target, quality=95, subsampling=0)
