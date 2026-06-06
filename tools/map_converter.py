#!/usr/bin/env python3
"""
GTAV-PS2 Map Converter
======================
Reads CodeWalker XML exports of GTA V map data
and converts them into our PS2 streaming cell format.

How to export from CodeWalker:
1. Open CodeWalker
2. Navigate to area you want
3. Tools → Export → Export XML
4. Point this script at the exported folder

Output: cells/cell_XX_YY.ps2c (our custom format)

Cell format (.ps2c):
  Header: magic(4) + cell_x(2) + cell_y(2) + num_verts(4) + num_quads(4)
  Verts:  array of (x,y,z) float32 * num_verts
  Quads:  array of (v0,v1,v2,v3,r,g,b) uint16*4 + uint8*3 * num_quads
"""

import struct
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# Cell grid config — matches our C code exactly
CELL_SIZE   = 100.0   # meters per cell
GRID_W      = 32
GRID_H      = 32
MAX_VERTS   = 4096    # per cell
MAX_QUADS   = 2048    # per cell
MAGIC       = b'PS2C'

# GTA V map origin offset (Los Santos center)
MAP_ORIGIN_X = -2000.0
MAP_ORIGIN_Z = -3000.0

def world_to_cell(x, z):
    cx = int((x - MAP_ORIGIN_X) / CELL_SIZE)
    cy = int((z - MAP_ORIGIN_Z) / CELL_SIZE)
    cx = max(0, min(cx, GRID_W-1))
    cy = max(0, min(cy, GRID_H-1))
    return cx, cy

def compress_texture_to_ps2_palette(r, g, b):
    """
    Compress PC texture colour to PS2 64x64 palette entry.
    PS2 uses 8bpp palettised textures — we quantise to 256 colours.
    """
    r = (r >> 3) << 3   # 5-bit precision
    g = (g >> 3) << 3
    b = (b >> 3) << 3
    return r, g, b

class Cell:
    def __init__(self, cx, cy):
        self.cx = cx
        self.cy = cy
        self.verts = []
        self.quads = []

    def add_quad(self, v0, v1, v2, v3, r, g, b):
        if len(self.verts) + 4 > MAX_VERTS: return
        if len(self.quads) >= MAX_QUADS: return
        base = len(self.verts)
        self.verts.extend([v0, v1, v2, v3])
        r, g, b = compress_texture_to_ps2_palette(r, g, b)
        self.quads.append((base, base+1, base+2, base+3, r, g, b))

    def size_bytes(self):
        return (16 +                      # header
                len(self.verts)*12 +      # 3 floats per vert
                len(self.quads)*11)       # 4 uint16 + 3 uint8

    def write(self, path):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'wb') as f:
            # Header
            f.write(MAGIC)
            f.write(struct.pack('<HH', self.cx, self.cy))
            f.write(struct.pack('<II', len(self.verts), len(self.quads)))
            # Vertices
            for v in self.verts:
                f.write(struct.pack('<fff', v[0], v[1], v[2]))
            # Quads
            for q in self.quads:
                f.write(struct.pack('<HHHH', q[0],q[1],q[2],q[3]))
                f.write(struct.pack('<BBB', q[4],q[5],q[6]))

def building_to_quads(x, y, z, w, h, d, r, g, b):
    """Convert a building box to 5 quads (front,back,left,right,top)"""
    hw, hd = w*0.5, d*0.5
    quads = []
    # Front face
    quads.append((
        (x-hw, 0,   z-hd), (x+hw, 0,   z-hd),
        (x+hw, h,   z-hd), (x-hw, h,   z-hd),
        r, g, b))
    # Back face
    quads.append((
        (x-hw, 0,   z+hd), (x+hw, 0,   z+hd),
        (x+hw, h,   z+hd), (x-hw, h,   z+hd),
        r*7//10, g*7//10, b*7//10))
    # Left face
    quads.append((
        (x-hw, 0,   z-hd), (x-hw, 0,   z+hd),
        (x-hw, h,   z+hd), (x-hw, h,   z-hd),
        r*6//10, g*6//10, b*6//10))
    # Right face
    quads.append((
        (x+hw, 0,   z-hd), (x+hw, 0,   z+hd),
        (x+hw, h,   z+hd), (x+hw, h,   z-hd),
        r*6//10, g*6//10, b*6//10))
    # Top face
    quads.append((
        (x-hw, h,   z-hd), (x+hw, h,   z-hd),
        (x+hw, h,   z+hd), (x-hw, h,   z+hd),
        min(r+20,255), min(g+20,255), min(b+20,255)))
    return quads

def process_codewalker_xml(xml_path, cells):
    """Parse a CodeWalker XML export and add geometry to cells."""
    try:
        tree = ET.parse(xml_path)
        root = tree.getroot()
    except Exception as e:
        print(f"  Skip {xml_path}: {e}")
        return 0

    count = 0
    for entity in root.iter('CEntityDef'):
        # Get position
        pos = entity.find('position')
        if pos is None: continue
        try:
            x = float(pos.get('x', 0))
            y = float(pos.get('y', 0))
            z = float(pos.get('z', 0))
        except: continue

        # Get archetype name for colour hint
        arch = entity.find('archetypeName')
        name = arch.text.lower() if arch is not None and arch.text else ''

        # Estimate building dimensions from name patterns
        if any(k in name for k in ['building','hotel','office','tower','block']):
            w = 20.0; h = 30.0; d = 20.0
            r, g, b = 85, 80, 75
        elif any(k in name for k in ['house','bungalow','villa','mansion']):
            w = 15.0; h = 8.0; d = 12.0
            r, g, b = 180, 140, 100
        elif any(k in name for k in ['shop','store','retail']):
            w = 10.0; h = 5.0; d = 8.0
            r, g, b = 90, 85, 80
        elif any(k in name for k in ['warehouse','industrial','factory']):
            w = 30.0; h = 12.0; d = 25.0
            r, g, b = 70, 68, 65
        else:
            continue  # skip props, vegetation etc

        cx, cy = world_to_cell(x, z)
        key = (cx, cy)
        if key not in cells:
            cells[key] = Cell(cx, cy)

        for q in building_to_quads(x, y, z, w, h, d, r, g, b):
            cells[key].add_quad(q[0],q[1],q[2],q[3],q[4],q[5],q[6])
        count += 1

    return count

def main():
    if len(sys.argv) < 2:
        print("Usage: map_converter.py <codewalker_xml_folder> [output_folder]")
        print("")
        print("How to export from CodeWalker:")
        print("  1. Open CodeWalker, load GTA V")
        print("  2. File -> Export Map XML")
        print("  3. Point this script at the exported folder")
        sys.exit(1)

    input_dir  = Path(sys.argv[1])
    output_dir = Path(sys.argv[2]) if len(sys.argv)>2 else Path('cells')

    print(f"GTAV-PS2 Map Converter")
    print(f"======================")
    print(f"Input:  {input_dir}")
    print(f"Output: {output_dir}")
    print(f"Grid:   {GRID_W}x{GRID_H} cells ({CELL_SIZE}m each)")
    print()

    cells = {}
    xml_files = list(input_dir.glob('**/*.xml'))
    print(f"Found {len(xml_files)} XML files")

    total = 0
    for i, xml in enumerate(xml_files):
        print(f"  [{i+1}/{len(xml_files)}] {xml.name}...", end='', flush=True)
        n = process_codewalker_xml(xml, cells)
        print(f" {n} buildings")
        total += n

    print(f"\nTotal buildings processed: {total}")
    print(f"Cells generated: {len(cells)}")

    # Write cells
    total_bytes = 0
    for (cx,cy), cell in cells.items():
        path = output_dir / f"cell_{cx:02d}_{cy:02d}.ps2c"
        cell.write(str(path))
        nb = cell.size_bytes()
        total_bytes += nb
        print(f"  cell_{cx:02d}_{cy:02d}: {len(cell.verts)} verts, "
              f"{len(cell.quads)} quads, {nb//1024}KB")

    print(f"\nTotal map data: {total_bytes//1024}KB "
          f"({total_bytes//1024//1024}MB)")
    print(f"Average per cell: {total_bytes//max(len(cells),1)//1024}KB")
    print(f"\nDone! Load cells into PS2 engine with stream_load_cell()")

if __name__ == '__main__':
    main()
