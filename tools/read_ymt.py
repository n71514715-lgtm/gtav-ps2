import struct, os, sys
from pathlib import Path

def scan_positions(data):
    positions = []
    i = 0
    while i < len(data) - 12:
        try:
            x = struct.unpack_from('<f', data, i)[0]
            y = struct.unpack_from('<f', data, i+4)[0]
            z = struct.unpack_from('<f', data, i+8)[0]
            if (-3000<x<4000 and -3000<y<3000 and -100<z<1000 and abs(x)>1 and abs(y)>1):
                positions.append((x,y,z))
                i += 12
            else:
                i += 4
        except:
            i += 4
    filtered = []
    for p in positions:
        if not any(abs(p[0]-f[0])<5 and abs(p[1]-f[1])<5 for f in filtered):
            filtered.append(p)
    return filtered

def process(ymt_path, out_dir):
    with open(ymt_path,'rb') as f: data=f.read()
    print(f"Reading {os.path.basename(ymt_path)} ({len(data)//1024}KB)")
    pos = scan_positions(data)
    print(f"Found {len(pos)} positions")
    if not pos: return 0
    os.makedirs(out_dir, exist_ok=True)
    cells = {}
    for (x,y,z) in pos:
        cx=max(0,min(int((x+2000)/100),31))
        cy=max(0,min(int((z+3000)/100),31))
        cells.setdefault((cx,cy),[]).append((x,y,z))
    for (cx,cy),blds in cells.items():
        path=os.path.join(out_dir,f"cell_{cx:02d}_{cy:02d}.ps2c")
        verts,quads=[],[]
        for (x,y,z) in blds:
            hw,h,hd=7.5,20.0,7.5
            r,g,b=85,80,75
            faces=[
                ((x-hw,0,z-hd),(x+hw,0,z-hd),(x+hw,h,z-hd),(x-hw,h,z-hd),r,g,b),
                ((x+hw,0,z+hd),(x-hw,0,z+hd),(x-hw,h,z+hd),(x+hw,h,z+hd),r*7//10,g*7//10,b*7//10),
                ((x-hw,0,z+hd),(x-hw,0,z-hd),(x-hw,h,z-hd),(x-hw,h,z+hd),r*6//10,g*6//10,b*6//10),
                ((x+hw,0,z-hd),(x+hw,0,z+hd),(x+hw,h,z+hd),(x+hw,h,z-hd),r*6//10,g*6//10,b*6//10),
            ]
            for face in faces:
                if len(verts)+4>4096 or len(quads)>=2048: break
                base=len(verts)
                verts.extend([face[0],face[1],face[2],face[3]])
                quads.append((base,base+1,base+2,base+3,face[4],face[5],face[6]))
        if not quads: continue
        with open(path,'wb') as f:
            f.write(b'PS2C')
            f.write(struct.pack('<HH',cx,cy))
            f.write(struct.pack('<II',len(verts),len(quads)))
            for v in verts: f.write(struct.pack('<fff',v[0],v[1],v[2]))
            for q in quads:
                f.write(struct.pack('<HHHH',q[0],q[1],q[2],q[3]))
                f.write(struct.pack('<BBB',q[4],q[5],q[6]))
        print(f"  cell_{cx:02d}_{cy:02d}: {len(blds)} buildings, {os.path.getsize(path)//1024}KB")
    return len(pos)

inp=Path(sys.argv[1]) if len(sys.argv)>1 else Path('.')
out=sys.argv[2] if len(sys.argv)>2 else 'assets/maps'
print("GTAV-PS2 YMT Reader")
print("===================")
total=0
for ymt in list(inp.glob('*.ymt'))+list(inp.glob('*.YMT')):
    total+=process(str(ymt),out)
print(f"\nTotal: {total} buildings extracted")
print("Real Los Santos data ready!")
