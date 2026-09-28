"""Package pre-rendered boot frames in Android's uncompressed ZIP format."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_STORED

ROOT = Path(__file__).resolve().parents[2]
frames = sorted((ROOT / 'out/branding/part0').glob('*.png'))
if [p.name for p in frames] != [f'{n:03}.png' for n in range(36)]:
    raise SystemExit('Expected exactly 36 consecutive boot frames')
destination = ROOT / 'device/aegis/qemu_arm64/branding/bootanimation.zip'
with ZipFile(destination, 'w', compression=ZIP_STORED) as archive:
    entries = [('desc.txt', b'720 720 24\np 0 0 part0 #F6F7F9\n')]
    entries += [(f'part0/{p.name}', p.read_bytes()) for p in frames]
    for name, data in entries:
        info = ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
        info.compress_type = ZIP_STORED
        info.external_attr = 0o100644 << 16
        archive.writestr(info, data)
print(destination)
