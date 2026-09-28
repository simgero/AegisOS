#!/usr/bin/env python3
"""Build an experimental sparse GPT disk from verified AegisOS images.

Only creates a new android.raw file. Never targets a physical device.
Boot and hardware-service integration is still under development.
"""
import struct, uuid, zlib, shutil
from pathlib import Path

import argparse
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('directory', type=Path, help='Contains images/, metadata.img, misc.img and frp.img; android.raw must not exist')
args = parser.parse_args()
root = args.directory.resolve()
images = root / 'images'
out = root / 'android.raw'
MiB = 1024**2

def sparse_header(path):
    with path.open('rb') as f:
        data = f.read(28)
    if len(data) == 28 and struct.unpack_from('<I', data)[0] == 0xed26ff3a:
        h = struct.unpack('<IHHHHIIII', data)
        if h[1:3] != (1, 0) or h[3] < 28 or h[4] < 12:
            raise ValueError('Unsupported sparse header')
        return h

def size(path):
    h = sparse_header(path)
    return h[5] * h[6] if h else path.stat().st_size

def copy_image(path, dest, start):
    h = sparse_header(path)
    dest.seek(start)
    with path.open('rb') as f:
        if not h:
            shutil.copyfileobj(f, dest, MiB)
            return
        f.seek(h[3]); blocks = 0
        for _ in range(h[7]):
            ch = f.read(h[4])
            kind, _, count, total = struct.unpack_from('<HHII', ch)
            length = count * h[5]
            if kind == 0xcac1:
                if total != h[4] + length: raise ValueError('RAW length')
                remaining = length
                while remaining:
                    data = f.read(min(MiB, remaining))
                    if not data: raise ValueError('Truncated sparse image')
                    dest.write(data); remaining -= len(data)
            elif kind == 0xcac2:
                if total != h[4] + 4: raise ValueError('FILL length')
                fill = f.read(4)
                if fill == b'\0'*4:
                    dest.seek(length, 1)
                else:
                    data = fill * (MiB//4)
                    while length:
                        n = min(length, len(data)); dest.write(data[:n]); length -= n
            elif kind == 0xcac3:
                if total != h[4]: raise ValueError('SKIP length')
                dest.seek(length, 1)
            elif kind == 0xcac4:
                if total != h[4]+4 or count: raise ValueError('CRC chunk')
                f.read(4)
            else: raise ValueError('Unknown sparse chunk')
            blocks += count
        if blocks != h[6]: raise ValueError('Sparse block count mismatch')

parts = [(n+'_a', images/(n+'.img')) for n in
         ['boot','init_boot','vendor_boot','vbmeta','vbmeta_system','vbmeta_system_dlkm','vbmeta_vendor_dlkm']]
parts += [('super',images/'super.img'),('userdata',images/'userdata.img'),
          ('metadata',root/'metadata.img'),('misc',root/'misc.img'),('frp',root/'frp.img')]
cursor = MiB//512
layout = []
for name, path in parts:
    sectors = ((size(path)+MiB-1)//MiB)*MiB//512
    layout.append((name,path,cursor,cursor+sectors-1))
    cursor += sectors
sectors = cursor+2048
table = bytearray(128*128)
kind = uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4').bytes_le
for i,(name,path,start,end) in enumerate(layout):
    struct.pack_into('<16s16sQQQ72s',table,i*128,kind,uuid.uuid4().bytes_le,start,end,0,name.encode('utf-16le'))
guid = uuid.uuid4().bytes_le
def header(current, backup, entries):
    data = bytearray(struct.pack('<8sIIIIQQQQ16sQIII', b'EFI PART',0x10000,92,0,0,
                                current,backup,34,sectors-34,guid,entries,128,128,zlib.crc32(table)))
    struct.pack_into('<I',data,16,zlib.crc32(data))
    return data.ljust(512,b'\0')
with out.open('xb') as dest:
    dest.truncate(sectors*512)
    mbr = bytearray(512)
    struct.pack_into('<B3sB3sII',mbr,446,0,b'\0\2\0',0xee,b'\xff'*3,1,min(sectors-1,0xffffffff))
    mbr[510:512] = b'\x55\xaa'
    dest.write(mbr);dest.write(header(1,sectors-1,2));dest.write(table)
    dest.seek((sectors-33)*512);dest.write(table);dest.write(header(sectors-1,1,sectors-33))
    for name,path,start,end in layout:
        print(name,start,end,flush=True)
        copy_image(path,dest,start*512)
print(out)
