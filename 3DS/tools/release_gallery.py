#!/usr/bin/env python3
"""Build release galleries from the game's reference rasterizer, not mockups."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
root = Path(__file__).resolve().parents[1] / 'preview'
font_path = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
try:
    title = ImageFont.truetype(font_path, 30)
    label = ImageFont.truetype(font_path, 23)
    small = ImageFont.truetype(font_path, 19)
except OSError:
    title = label = small = ImageFont.load_default()

cards = [(1, 'LIT WINDOWS / DAMAGE-DRIVEN PURSUITS'),
         (23, 'NEON MECHANICS / NEW CITY BUSINESSES'),
         (11, 'MIDNIGHT EXPRESS / 32 GARAGE RIDES'),
         (13, 'DOUBLE DECKER / LIVE PREVIEWS'),
         (15, 'SIDEWALK SURFER / UNCONVENTIONAL RIDES'),
         (24, 'DRIFT SCORE / CASH PAID WHEN CAUGHT')]
out = Image.new('RGB', (1640, 1760), '#080d18')
d = ImageDraw.Draw(out)
d.text((20, 18), 'COASTLINE CHASE  /  v0.4', font=title, fill='#63ffff')
d.text((20, 60), 'Staged 400 x 240 game renders. Hardware performance remains unverified.', font=small, fill='#91a7bd')
for k, (scene, text) in enumerate(cards):
    x, y = 20 + k % 2 * 820, 106 + k // 2 * 540
    d.text((x, y), text, font=label, fill='#d7eaff')
    im = Image.open(root / f'coast_{scene}_top.ppm').resize((800, 480), Image.Resampling.NEAREST)
    out.paste(im, (x, y + 38))
out.save(root / 'release_preview.png')

atlas = Image.new('RGB', (1320, 570), '#080d18')
d = ImageDraw.Draw(atlas)
d.text((12, 8), 'COASTLINE CHASE / EIGHT NAMED PROCEDURAL ISLANDS', font=label, fill='#63ffff')
for i in range(8):
    path = root / f'city_{i}.ppm'
    if path.exists():
        atlas.paste(Image.open(path), (10 + i % 4 * 330, 48 + i // 4 * 260))
atlas.save(root / 'city_atlas.png')

rides = Image.new('RGB', (1640, 890), '#080d18')
d = ImageDraw.Draw(rides)
d.text((12, 10), 'TWELVE NEW RIDES / LIVE GARAGE PREVIEWS', font=label, fill='#63ffff')
for i in range(12):
    rides.paste(Image.open(root / f'coast_{11+i}_top.ppm'), (10 + i % 4 * 410, 48 + i // 4 * 280))
rides.save(root / 'new_rides.png')
