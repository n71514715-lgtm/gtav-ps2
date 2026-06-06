#!/usr/bin/env python3
"""
GTAV-PS2 YMap Extractor
========================
Extracts building positions from GTA V's YMap files.
These are the actual building placement files used by RAGE.

YMap files live inside RPF archives.
OpenIV can extract them — export as XML first.

How to use:
1. Open OpenIV
2. Navigate to x64e.rpf/levels/gta5/citye/
3. Select all .ymap files
4. Right click -> Export as XML
5. Save to C:\gtav_export\ymaps\
6. Run: python3 extract_ymaps.py /mnt/c/gtav_export/ymaps/
"""

import xml.etree.ElementTree as ET
import struct
import os
import sys
from pathlib import Path

CELL_SIZE    = 100.0
MAP_ORIGIN_X = -2000.0
MAP_ORIGIN_Z = -3000.0
GRID_W       = 32
GRID_H       = 32

def world_to_cell(x, z):
    cx = int((x - MAP_ORIGIN_X) / CELL_SIZE)
    cy = int((z - MAP_ORIGIN_Z) / CELL_SIZE)
    return max(0,min(cx,GRID_W-1)), max(0,min(cy,GRID_H-1))

# Building name → PS2 colour + size hints
ARCH_HINTS = {
    'hotel':      (85,80,75,  20,30,20),
    'office':     (90,85,82,  15,25,15),
    'tower':      (80,78,75,  18,40,18),
    'building':   (82,78,74,  20,20,20),
    'house':      (175,140,100,12,7,10),
    'mansion':    (190,155,110,20,9,16),
    'shop':       (88,84,80,  10,5,8),
    'warehouse':  (70,68,65,  30,12,25),
    'bank':       (95,90,85,  18,20,15),
    'police':     (70,75,90,  20,15,18),
    'hospital':   (200,195,190,25,20,22),
    'garage':     (75,72,70,  15,8,12),
    'store':      (85,82,78,  12,5,10),
}

def get_hints(name):
    n = name.lower()
    for k,(r,g,b,w,h,d) in ARCH_HINTS.items():
        if k in n:
            return r,g,b,w,h,d
    return 82,78,74,15,15,15  # default building

buildings_by_cell = {}

def parse_ymap(path):
    try:
        tree = ET.parse(path)
        root = tree.getroot()
    except:
        return 0

    count = 0
    # CMapData/entities/Item entries
    for item in root.iter('Item'):
        arch = item.find('archetypeName')
        if arch is None: continue

        pos_elem = item.find('position')
        if pos_elem is None: continue

        try:
            x = float(pos_elem.get('x',0))
            y = float(pos_elem.get('y',0))
            z = float(pos_elem.get('z',0))
        except:
            continue

        name = arch.text or ''
        r,g,b,w,h,d = get_hints(name)

        # Only include actual buildings (filter small props)
        lod = item.find('lodDist')
        if lod is not None:
            try:
                if float(lod.text) < 50: continue  # skip small props
            except: pass

        cx,cy = world_to_cell(x,z)
        key = (cx,cy)
        if key not in buildings_by_cell:
            buildings_by_cell[key] = []
        buildings_by_cell[key].append((x,y,z,w,h,d,r,g,b,name))
        count += 1

    return count

def write_cells(output_dir):
    os.makedirs(output_dir, exist_ok=True)
    total_kb = 0

    for (cx,cy), blds in sorted(buildings_by_cell.items()):
        path = os.path.join(output_dir, f"cell_{cx:02d}_{cy:02d}.ps2c")
        verts = []
        quads = []

        for (x,y,z,w,h,d,r,g,b,name) in blds:
            hw,hd = w*.5,d*.5
            # 5 faces per building
            faces = [
                # front
                ((x-hw,0,z-hd),(x+hw,0,z-hd),(x+hw,h,z-hd),(x-hw,h,z-hd),r,g,b),
                # back
                ((x+hw,0,z+hd),(x-hw,0,z+hd),(x-hw,h,z+hd),(x+hw,h,z+hd),r*7//10,g*7//10,b*7//10),
                # left
                ((x-hw,0,z+hd),(x-hw,0,z-hd),(x-hw,h,z-hd),(x-hw,h,z+hd),r*6//10,g*6//10,b*6//10),
                # right
                ((x+hw,0,z-hd),(x+hw,0,z+hd),(x+hw,h,z+hd),(x+hw,h,z-hd),r*6//10,g*6//10,b*6//10),
                # top
                ((x-hw,h,z-hd),(x+hw,h,z-hd),(x+hw,h,z+hd),(x-hw,h,z+hd),min(r+20,255),min(g+20,255),min(b+20,255)),
            ]

            for f in faces:
                if len(verts)+4 > 4096 or len(quads) >= 2048:
                    break
                base = len(verts)
                verts.extend([f[0],f[1],f[2],f[3]])
                quads.append((base,base+1,base+2,base+3,f[4],f[5],f[6]))

        if not quads: continue

        with open(path,'wb') as f:
            f.write(b'PS2C')
            f.write(struct.pack('<HH',cx,cy))
            f.write(struct.pack('<II',len(verts),len(quads)))
            for v in verts:
                f.write(struct.pack('<fff',v[0],v[1],v[2]))
            for q in quads:
                f.write(struct.pack('<HHHH',q[0],q[1],q[2],q[3]))
                f.write(struct.pack('<BBB',q[4],q[5],q[6]))

        kb = os.path.getsize(path)//1024
        total_kb += kb
        print(f"  cell_{cx:02d}_{cy:02d}: {len(blds)} buildings, "
              f"{len(verts)} verts, {kb}KB")

    return total_kb

def main():
    if len(sys.argv) < 2:
        print("Usage: extract_ymaps.py <ymap_xml_folder> [output_folder]")
        print()
        print("Export YMAPs from OpenIV:")
        print("  x64e.rpf/levels/gta5/citye/ -> Export as XML")
        sys.exit(1)

    inp = Path(sys.argv[1])
    out = sys.argv[2] if len(sys.argv)>2 else 'assets/maps'

    print("GTAV-PS2 YMap Extractor")
    print("=======================")

    xmls = list(inp.glob('**/*.xml')) + list(inp.glob('**/*.ymap.xml'))
    print(f"Found {len(xmls)} files in {inp}")

    total = 0
    for i,f in enumerate(xmls):
        n = parse_ymap(f)
        total += n
        if n > 0:
            print(f"  {f.name}: {n} buildings")

    print(f"\nTotal: {total} buildings across {len(buildings_by_cell)} cells")
    kb = write_cells(out)
    print(f"Map data: {kb}KB ({kb//1024}MB)")
    print(f"\nNext: copy cells/ to PS2 DVD image")
    print("Done!")

if __name__ == '__main__':
    main()
