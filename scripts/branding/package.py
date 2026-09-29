"""Validate and package the approved intro + loop in Android's stored-ZIP format."""
from pathlib import Path
from zipfile import ZipFile, ZipInfo, ZIP_STORED
from PIL import Image
import io,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
source=ROOT/'out/branding/production'
meta=json.loads((source/'manifest.json').read_text())
assert meta==dict(width=720,height=720,fps=30,introFrames=158,loopFrames=126,loopSeconds=4.2)
entries=[]
for part,count in [('part0',158),('part1',126)]:
 frames=sorted((source/part).glob('*.png'))
 if [p.name for p in frames]!=[f'{i:04}.png' for i in range(count)]:raise ValueError(f'Incomplete {part}')
 for p in frames:
  with Image.open(p) as im:
   im=im.convert('RGB');assert im.size==(2160,2160)
   im=im.resize((720,720),Image.Resampling.LANCZOS)
   assert all(im.getpixel(pos)==(0,0,0) for pos in [(0,0),(719,0),(0,719),(719,719)])
   # Lossless RGB optimisation preserves exact black and antialiasing.
   buffer=io.BytesIO();im.save(buffer,format='PNG',optimize=True)
   entries.append((f'{part}/{p.name}',buffer.getvalue()))
def write(name,description,selected):
 destination=ROOT/'device/aegis/qemu_arm64/branding'/name
 buffer=io.BytesIO()
 with ZipFile(buffer,'w',compression=ZIP_STORED) as archive:
  for filename,data in [('desc.txt',description.encode()),*selected]:
   info=ZipInfo(filename,date_time=(2026,1,1,0,0,0));info.compress_type=ZIP_STORED;info.external_attr=0o100644<<16
   archive.writestr(info,data)
 data=buffer.getvalue()
 with ZipFile(io.BytesIO(data)) as archive:assert archive.testzip() is None
 if len(data)>8*1024*1024:raise ValueError('Archive exceeds build-input file limit')
 temporary=destination.with_suffix('.zip.tmp')
 temporary.write_bytes(data);temporary.replace(destination)
 print(f'{destination.name}: {destination.stat().st_size} bytes, sha256={hashlib.sha256(destination.read_bytes()).hexdigest()}')
write('bootanimation.zip','720 720 30\np 1 0 part0 #000000\np 0 0 part1 #000000\n',entries)
# Shutdown uses the same approved loop, without replaying startup construction.
write('shutdownanimation.zip','720 720 30\np 0 0 part1 #000000\n',[e for e in entries if e[0].startswith('part1/')])
