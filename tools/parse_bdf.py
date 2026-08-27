import re
import sys

def parse_bdf(filepath, expected_w, expected_h, start_char=32, end_char=126):
    glyphs = {}
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    char_blocks = re.findall(r'STARTCHAR\s+(.*?)\n(.*?)ENDCHAR', content, re.DOTALL)
    for name, block in char_blocks:
        enc_match = re.search(r'ENCODING\s+(\d+)', block)
        if not enc_match:
            continue
        enc = int(enc_match.group(1))
        if enc < start_char or enc > end_char:
            continue

        bbx_match = re.search(r'BBX\s+(\d+)\s+(\d+)\s+([-\d]+)\s+([-\d]+)', block)
        bitmap_match = re.search(r'BITMAP\s+(.*)', block, re.DOTALL)
        if not bbx_match or not bitmap_match:
            continue

        bbw = int(bbx_match.group(1))
        bbh = int(bbx_match.group(2))
        xoff = int(bbx_match.group(3))
        yoff = int(bbx_match.group(4))

        hex_lines = bitmap_match.group(1).strip().split()
        rows = []
        for line in hex_lines:
            val = int(line, 16)
            rows.append(val)

        # We need to position bbw x bbh within expected_w x expected_h
        # Standard baseline for 6x10 is descent ~2, ascent ~8
        # Standard baseline for 8x13 is descent ~3, ascent ~10
        ascent = expected_h - (2 if expected_h == 10 else 3)
        grid = [0] * expected_h

        # y in BDF: 0 is baseline, yoff is offset of bottom of glyph box
        # row index from top: expected_h - 1 - (y)
        # So top of glyph is at y = yoff + bbh - 1
        # row_top = ascent - (yoff + bbh)
        for r, row_val in enumerate(rows):
            # The row index in grid
            gly_y = (bbh - 1 - r) + yoff
            grid_y = ascent - gly_y - 1
            if 0 <= grid_y < expected_h:
                # Shift by xoff if needed
                if xoff >= 0:
                    shifted = row_val >> xoff
                else:
                    shifted = row_val << (-xoff)
                grid[grid_y] = (shifted >> (8 - expected_w if expected_w <= 8 else 0)) & 0xFF
                if expected_w == 6:
                    grid[grid_y] = (row_val) & 0xFC # 6 MSB bits

        glyphs[enc] = grid

    return glyphs

def generate_c(font_name, glyphs, w, h, start_char=32, end_char=126):
    out = []
    out.append('#include "font.h"')
    out.append('')
    out.append(f'// {font_name}: {w}x{h}, ASCII {start_char} to {end_char}')
    out.append(f'static const uint8_t {font_name}_data[] = {{')

    for c in range(start_char, end_char + 1):
        char_repr = chr(c) if c != 92 and c != 39 and c != 34 else f"\\{chr(c)}"
        grid = glyphs.get(c, [0] * h)
        hex_vals = ', '.join(f'0x{v:02X}' for v in grid)
        out.append(f'    {hex_vals}, // {c:3d} (\'{char_repr}\')')

    out.append('};')
    out.append('')
    out.append(f'const font_t {font_name} = {{')
    out.append(f'    .width = {w},')
    out.append(f'    .height = {h},')
    out.append(f'    .first_char = {start_char},')
    out.append(f'    .last_char = {end_char},')
    out.append(f'    .data = {font_name}_data')
    out.append('};')
    out.append('')
    return '\n'.join(out)

if __name__ == '__main__':
    import sys
    bdf6 = sys.argv[1] if len(sys.argv) > 1 else 'font6.bdf'
    bdf8 = sys.argv[2] if len(sys.argv) > 2 else 'font8.bdf'

    g6 = parse_bdf(bdf6, 6, 10)
    c6 = generate_c('font_6x10', g6, 6, 10)
    with open('src/fonts/font_6x10.c', 'w') as f:
        f.write(c6)

    g8 = parse_bdf(bdf8, 8, 13)
    c8 = generate_c('font_8x13_bold', g8, 8, 13)
    with open('src/fonts/font_8x13_bold.c', 'w') as f:
        f.write(c8)

    print(f"Generated font_6x10.c with {len(g6)} glyphs and font_8x13_bold.c with {len(g8)} glyphs")
