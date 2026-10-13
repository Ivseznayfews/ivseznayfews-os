#!/usr/bin/env python3
"""Regenerate the checked-in 8x16 ASCII bitmap (requires Pillow and DejaVu)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
font = ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf', 13)
output = ['// Generated from DejaVu Sans Mono; see licenses/DejaVu.txt.',
          '#pragma once', '#include <stdint.h>', 'static const uint8_t font8x16[95][16] = {']
for code in range(32, 127):
    bitmap = Image.new('1', (8, 16))
    ImageDraw.Draw(bitmap).text((0, 0), chr(code), font=font, fill=1, stroke_width=0)
    rows = [sum((1 << (7-x)) if bitmap.getpixel((x,y)) else 0 for x in range(8)) for y in range(16)]
    output.append('    {' + ','.join(f'0x{row:02x}' for row in rows) + '},')
output.append('};')
(Path(__file__).resolve().parents[1] / 'kernel/font.h').write_text('\n'.join(output) + '\n')
