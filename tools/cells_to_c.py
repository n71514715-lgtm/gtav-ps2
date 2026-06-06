#!/usr/bin/env python3
"""Convert PS2C cell files to a C header for embedding in the engine."""
import struct, os, glob, sys

cells_dir = sys.argv[1] if len(sys.argv)>1 else 'assets/maps'
out_file  = sys.argv[2] if len(sys.argv)>2 else 'src/world/map_data.h'

files = glob.glob(os.path.join(cells_dir, 'cell_*.ps2c'))
print(f"Found {len(files)} cell files")

buildings = []
for path in files:
    with open(path,'rb') as f:
        magic = f.read(4)
        if magic != b'PS2C': continue
        cx,cy = struct.unpack('<HH', f.read(4))
        nv,nq = struct.unpack("<II", f.read(8))
        verts = []
        for _ in range(nv):
            x,y,z = struct.unpack('<fff', f.read(12))
            verts.append((x,y,z))
        for _ in range(nq):
            v0,v1,v2,v3 = struct.unpack('<HHHH', f.read(8))
            r,g,b = struct.unpack('<BBB', f.read(3))
            # Get centre from first vertex
            if v0 < len(verts):
                vx = verts[v0][0]
                vy = verts[v0][2]  # z is depth
                buildings.append((vx, vy, 15.0, 20.0, 15.0, r, g, b))

print(f"Total buildings: {len(buildings)}")

os.makedirs(os.path.dirname(out_file), exist_ok=True)
with open(out_file,'w') as f:
    f.write("/* AUTO-GENERATED from real GTA V YMT data */\n")
    f.write("/* Rockford Hills — Michael's neighborhood  */\n\n")
    f.write(f"#define NUM_REAL_BUILDINGS {len(buildings)}\n\n")
    f.write("static Bld real_buildings[] = {\n")
    for (x,z,w,h,d,r,g,b) in buildings:
        f.write(f"    {{{x:.1f},{z:.1f},{w:.1f},{h:.1f},{d:.1f},{r},{g},{b}}},\n")
    f.write("};\n")

print(f"Written to {out_file}")
print("Real Los Santos ready to render!")
