#!/usr/bin/env python3
"""Generate a simple anvil icon as a 32x32 PNG for the Nexus quick access bar."""
import struct, zlib

W, H = 32, 32

# RGBA pixels - start transparent
pixels = [[(0,0,0,0) for _ in range(W)] for _ in range(H)]

# Colors
BODY = (180, 180, 190, 255)      # Steel grey
HIGHLIGHT = (210, 210, 220, 255)  # Light steel
SHADOW = (120, 120, 130, 255)     # Dark steel
DARK = (80, 80, 90, 255)          # Very dark
BASE = (100, 100, 110, 255)       # Base color

def fill_rect(px, x1, y1, x2, y2, color):
    for y in range(y1, y2+1):
        for x in range(x1, x2+1):
            if 0 <= x < W and 0 <= y < H:
                px[y][x] = color

# Anvil shape (simple side-view silhouette):
# Horn (pointed left side, rows 6-10)
fill_rect(pixels, 4, 7, 9, 9, SHADOW)
fill_rect(pixels, 5, 8, 8, 8, BODY)

# Face/top plate (wide flat surface, rows 5-10)
fill_rect(pixels, 8, 5, 26, 10, BODY)
fill_rect(pixels, 8, 5, 26, 6, HIGHLIGHT)   # top highlight
fill_rect(pixels, 8, 9, 26, 10, SHADOW)     # bottom shadow

# Neck/waist (narrower, rows 11-16)
fill_rect(pixels, 12, 11, 22, 17, BODY)
fill_rect(pixels, 12, 11, 13, 17, HIGHLIGHT) # left highlight
fill_rect(pixels, 21, 11, 22, 17, SHADOW)    # right shadow

# Feet/base (wide, rows 17-22)
fill_rect(pixels, 6, 18, 28, 20, BODY)
fill_rect(pixels, 6, 18, 28, 18, HIGHLIGHT)
fill_rect(pixels, 6, 20, 28, 20, SHADOW)

# Bottom base plate
fill_rect(pixels, 4, 21, 30, 24, DARK)
fill_rect(pixels, 4, 21, 30, 21, SHADOW)
fill_rect(pixels, 4, 24, 30, 24, DARK)

# Rounded horn tip (taper)
fill_rect(pixels, 3, 8, 4, 8, SHADOW)

# Write PNG
def make_png(w, h, pixels):
    def chunk(ctype, data):
        c = ctype + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xFFFFFFFF)
    
    raw = b''
    for row in pixels:
        raw += b'\x00'  # filter none
        for r, g, b, a in row:
            raw += struct.pack('BBBB', r, g, b, a)
    
    ihdr = struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)  # 8-bit RGBA
    
    sig = b'\x89PNG\r\n\x1a\n'
    return sig + chunk(b'IHDR', ihdr) + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b'')

png_data = make_png(W, H, pixels)

# Save as file
with open('data/CraftyLegend/icon_anvil.png', 'wb') as f:
    f.write(png_data)

# Also save hover version (slightly brighter)
pixels_hover = [[(0,0,0,0) for _ in range(W)] for _ in range(H)]
for y in range(H):
    for x in range(W):
        r, g, b, a = pixels[y][x]
        if a > 0:
            r = min(255, r + 40)
            g = min(255, g + 40)
            b = min(255, b + 40)
        pixels_hover[y][x] = (r, g, b, a)

png_hover = make_png(W, H, pixels_hover)
with open('data/CraftyLegend/icon_anvil_hover.png', 'wb') as f:
    f.write(png_hover)

# Generate C array
def to_c_array(data, name):
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_vals = ', '.join(f'0x{b:02x}' for b in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};")
    lines.append(f"static const unsigned int {name}_size = {len(data)};")
    return '\n'.join(lines)

print(to_c_array(png_data, "ICON_ANVIL"))
print()
print(to_c_array(png_hover, "ICON_ANVIL_HOVER"))
print(f"\n// Normal: {len(png_data)} bytes, Hover: {len(png_hover)} bytes")
