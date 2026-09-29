#!/usr/bin/env python3
"""Create a new GPT file from verified images, never a physical disk.

--reuse-disk clones an immutable base with APFS. Every new byte, including
holes and padding, is still generated and checked. Never seed from live data.
"""
import argparse
import importlib.util
import json
from pathlib import Path
import struct
import uuid
import zlib

MiB = 1024**2
MAX_DISK = 32 * 1024**3
IMAGE_NAMES = ['boot', 'init_boot', 'vendor_boot', 'vbmeta', 'vbmeta_system',
               'vbmeta_system_dlkm', 'vbmeta_vendor_dlkm']
spec = importlib.util.spec_from_file_location('aegis_disk_io', Path(__file__).with_name('local_image_io.py'))
image_io = importlib.util.module_from_spec(spec)
spec.loader.exec_module(image_io)


def read_exact(source, length):
    data = source.read(length)
    if len(data) != length:
        raise ValueError('Truncated image or sparse chunk')
    return data


def sparse_header(path):
    image_io.regular_source(path)
    with path.open('rb') as source:
        data = source.read(28)
    if len(data) >= 4 and struct.unpack_from('<I', data)[0] == 0xed26ff3a:
        if len(data) != 28:
            raise ValueError('Truncated sparse header')
        header = struct.unpack('<IHHHHIIII', data)
        if (header[1:3] != (1, 0) or not 28 <= header[3] <= 4096
                or not 12 <= header[4] <= 4096 or not 4 <= header[5] <= MiB
                or header[5] % 4 or not 0 < header[5] * header[6] <= MAX_DISK
                or not 0 < header[7] <= 1000000):
            raise ValueError('Unsupported sparse header')
        return header


def size(path):
    header = sparse_header(path)
    result = header[5] * header[6] if header else path.stat().st_size
    if not 0 < result <= MAX_DISK:
        raise ValueError('Invalid partition image length')
    return result


def copy_image(path, target):
    header = sparse_header(path)
    with path.open('rb') as source:
        if not header:
            for data in iter(lambda: source.read(MiB), b''):
                target.write(data)
            return
        read_exact(source, header[3])
        blocks = 0
        for _ in range(header[7]):
            chunk = read_exact(source, header[4])
            kind, _, count, total = struct.unpack_from('<HHII', chunk)
            length = count * header[5]
            if blocks + count > header[6]:
                raise ValueError('Sparse block count exceeds image')
            if kind == 0xcac1:
                if total != header[4] + length:
                    raise ValueError('RAW length')
                while length:
                    data = read_exact(source, min(MiB, length))
                    target.write(data)
                    length -= len(data)
            elif kind == 0xcac2:
                if total != header[4] + 4:
                    raise ValueError('FILL length')
                fill = read_exact(source, 4)
                if fill == bytes(4):
                    target.zeros(length)
                else:
                    data = fill * (MiB // 4)
                    while length:
                        count_bytes = min(length, len(data))
                        target.write(data[:count_bytes])
                        length -= count_bytes
            elif kind == 0xcac3:
                if total != header[4]:
                    raise ValueError('SKIP length')
                target.zeros(length)
            elif kind == 0xcac4:
                if total != header[4] + 4 or count:
                    raise ValueError('CRC chunk')
                read_exact(source, 4)
            else:
                raise ValueError('Unknown sparse chunk')
            blocks += count
        if blocks != header[6] or source.read(1):
            raise ValueError('Sparse block count or trailing-byte mismatch')


def make_disk(directory, reuse_disk=None, guid_factory=uuid.uuid4):
    root = Path(directory).absolute()
    if root.resolve() != root:
        raise ValueError('Disk directory must have no symlink components')
    images = root / 'images'
    out = root / 'android.raw'
    parts = [(name + '_a', images / (name + '.img')) for name in IMAGE_NAMES]
    parts += [('super', images / 'super.img'), ('userdata', images / 'userdata.img'),
              ('metadata', root / 'metadata.img'), ('misc', root / 'misc.img'), ('frp', root / 'frp.img')]
    cursor = MiB // 512
    layout = []
    for name, path in parts:
        image_size = size(path)
        sectors = ((image_size + MiB - 1) // MiB) * MiB // 512
        layout.append((name, path, cursor, cursor + sectors - 1, image_size))
        cursor += sectors
    sectors = cursor + 2048
    if sectors * 512 > MAX_DISK:
        raise ValueError('GPT disk exceeds size limit')
    table = bytearray(128 * 128)
    kind = uuid.UUID('0fc63daf-8483-4772-8e79-3d69d8477de4').bytes_le
    for index, (name, _, start, end, _) in enumerate(layout):
        struct.pack_into('<16s16sQQQ72s', table, index * 128, kind, guid_factory().bytes_le,
                         start, end, 0, name.encode('utf-16le'))
    guid = guid_factory().bytes_le

    def header(current, backup, entries):
        data = bytearray(struct.pack('<8sIIIIQQQQ16sQIII', b'EFI PART', 0x10000, 92, 0, 0,
                                    current, backup, 34, sectors - 34, guid, entries,
                                    128, 128, zlib.crc32(table)))
        struct.pack_into('<I', data, 16, zlib.crc32(data))
        return data.ljust(512, b'\0')

    with image_io.ImageWriter(out, sectors * 512, reuse_disk) as target:
        mbr = bytearray(512)
        struct.pack_into('<B3sB3sII', mbr, 446, 0, b'\0\2\0', 0xee, b'\xff' * 3,
                         1, min(sectors - 1, 0xffffffff))
        mbr[510:512] = b'\x55\xaa'
        target.write(mbr)
        target.write(header(1, sectors - 1, 2))
        target.write(table)
        for name, path, start, end, image_size in layout:
            target.zeros(start * 512 - target.position)
            copy_image(path, target)
            if target.position != start * 512 + image_size:
                raise ValueError('Partition image changed length during copying')
            target.zeros((end + 1) * 512 - target.position)
        target.zeros((sectors - 33) * 512 - target.position)
        target.write(table)
        target.write(header(sectors - 1, 1, sectors - 33))
        checked = target.finish()
        checked.update(cloned=target.cloned, bytes_written=target.written,
                       bytes_unchanged_or_zero=target.reused)
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--reuse-disk', type=Path, help='Immutable raw base to clone; existing targets are refused')
    args = parser.parse_args()
    report = make_disk(args.directory, args.reuse_disk)
    with (args.directory / 'android.raw.json').open('x') as receipt:
        json.dump(report, receipt, sort_keys=True, indent=2)
        receipt.write('\n')
    print(args.directory / 'android.raw')
    print(report['sha256'])


if __name__ == '__main__':
    main()
