#!/usr/bin/env python3
"""Generate 32x32 anvil icons matching Nexus quick access icon style.

Style: flat monochrome silhouette, muted steel-blue grey, transparent background.
Generates normal and hover variants, outputs as C byte arrays.
"""

from PIL import Image, ImageDraw
import io
import struct
import zlib

def draw_anvil(img, color):
    """Draw a bold anvil silhouette on a 32x32 image, matching Nexus icon style."""
    draw = ImageDraw.Draw(img)
    outline = (0, 0, 0, 255)
    dim = (max(color[0] - 25, 0), max(color[1] - 25, 0), max(color[2] - 25, 0), color[3])
    bright = (min(color[0] + 35, 255), min(color[1] + 35, 255), min(color[2] + 35, 255), color[3])

    # Helper: draw shape with black outline by drawing outline first, then fill inset
    def outlined_rect(bbox, fill_col):
        x0, y0, x1, y1 = bbox
        draw.rectangle([x0-1, y0-1, x1+1, y1+1], fill=outline)
        draw.rectangle(bbox, fill=fill_col)

    def outlined_poly(points, fill_col):
        # Draw outline by drawing polygon slightly expanded, then fill
        draw.polygon(points, fill=fill_col, outline=outline)

    # === ANVIL BODY (side profile, facing left) ===

    # Top working surface (wide, flat) - outline first
    draw.rectangle([4, 9, 27, 15], fill=outline)
    draw.rectangle([5, 10, 26, 14], fill=color)

    # Horn (left tapered point) - outline then fill
    draw.polygon([(5, 9), (5, 15), (0, 14), (0, 10)], fill=outline)
    draw.polygon([(5, 10), (5, 14), (1, 13), (1, 11)], fill=color)

    # Heel (right block, slightly taller) - outline then fill
    draw.rectangle([25, 8, 31, 16], fill=outline)
    draw.rectangle([26, 9, 30, 15], fill=color)

    # Hardy/pritchel hole hint on top
    draw.rectangle([23, 10, 25, 11], fill=dim)

    # Waist (narrower body beneath surface) - outline then fill
    draw.rectangle([9, 14, 24, 19], fill=outline)
    draw.rectangle([10, 15, 23, 18], fill=color)
    # Taper from surface to waist
    draw.polygon([(5, 14), (10, 15), (10, 14)], fill=outline)
    draw.polygon([(6, 14), (10, 15), (10, 14)], fill=color)
    draw.polygon([(26, 14), (23, 15), (23, 14)], fill=outline)
    draw.polygon([(25, 14), (23, 15), (23, 14)], fill=color)

    # Feet/base (wide, solid) - outline then fill
    draw.rectangle([5, 18, 28, 23], fill=outline)
    draw.rectangle([6, 19, 27, 22], fill=color)
    # Taper from waist to feet
    draw.polygon([(10, 18), (6, 19), (10, 19)], fill=color)
    draw.polygon([(23, 18), (27, 19), (23, 19)], fill=color)

    # Bottom platform - outline then fill
    draw.rectangle([3, 21, 30, 26], fill=outline)
    draw.rectangle([4, 22, 29, 25], fill=color)

    # Top surface highlight
    draw.line([(5, 10), (26, 10)], fill=bright)
    draw.line([(2, 11), (4, 11)], fill=bright)
    draw.line([(26, 9), (30, 9)], fill=bright)

    # === HAMMER (resting diagonally above anvil) ===
    # Hammer head outline then fill
    draw.polygon([(7, 0), (14, 0), (13, 6), (6, 6)], fill=outline)
    draw.polygon([(8, 1), (13, 1), (12, 5), (7, 5)], fill=color)
    # Hammer head highlight
    draw.line([(8, 1), (13, 1)], fill=bright)

    # Handle - outline then fill
    for offset in [-1, 2]:
        draw.line([(11 + offset, 2), (18 + offset, 8)], fill=outline, width=1)
    for offset in [0, 1]:
        draw.line([(11 + offset, 2), (18 + offset, 8)], fill=color, width=1)

    # === SPARK (small detail) ===
    draw.point((15, 9), fill=bright)
    draw.point((17, 8), fill=bright)

def generate_icon(color, scale=1.0):
    """Generate a 32x32 anvil icon with the given color.
    If scale > 1.0, draws on a larger canvas then downscales for a 'bigger' look."""
    if scale > 1.0:
        size = int(32 * scale)
        img = Image.new('RGBA', (size, size), (0, 0, 0, 0))
        draw_anvil(img, color)
        img = img.resize((32, 32), Image.LANCZOS)
    else:
        img = Image.new('RGBA', (32, 32), (0, 0, 0, 0))
        draw_anvil(img, color)
    return img

def image_to_c_array(img, name):
    """Convert PIL image to C byte array string."""
    buf = io.BytesIO()
    img.save(buf, format='PNG', optimize=True)
    data = buf.getvalue()
    
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_vals = ", ".join(f"0x{b:02x}" for b in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};")
    lines.append(f"static const unsigned int {name}_size = {len(data)};")
    return "\n".join(lines), len(data)

# Warm khaki/tan-grey matching other Nexus quick access icons
# Normal: muted warm grey-tan, like the inactive icons in the bar
normal_color = (148, 140, 124, 200)  # Warm grey-tan with slight transparency
# Hover: brighter warm tan, fully opaque
hover_color = (190, 182, 162, 255)   # Brighter warm tan fully opaque

normal_img = generate_icon(normal_color, scale=1.12)  # Slightly smaller (shrunk to fit)
hover_img = generate_icon(hover_color)  # Full size = appears larger on hover

# Save preview PNGs
normal_img.save("/home/tony/Dev/crafty_legend/scripts/anvil_normal.png")
hover_img.save("/home/tony/Dev/crafty_legend/scripts/anvil_hover.png")

# Generate C arrays
normal_c, normal_size = image_to_c_array(normal_img, "ICON_ANVIL")
hover_c, hover_size = image_to_c_array(hover_img, "ICON_ANVIL_HOVER")

print("// Embedded 32x32 anvil icon (normal - warm khaki-tan)")
print(normal_c)
print()
print("// Embedded 32x32 anvil icon (hover - brighter tan, slightly larger)")
print(hover_c)
print()
print(f"// Normal: {normal_size} bytes, Hover: {hover_size} bytes")
