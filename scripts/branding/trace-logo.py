"""Trace the approved raster's alpha outlines for the SVG motion study.
No bitmap is modified; output coordinates remain in the original 1254px space.
"""
from pathlib import Path
from PIL import Image
import json, math
root=Path(__file__).resolve().parents[2]
im=Image.open(root/'device/aegis/qemu_arm64/branding/logo.png').convert('RGBA')
w,h=im.size
alpha=im.getchannel('A'); px=alpha.load(); edges={}
def add(a,b): edges.setdefault(a,[]).append(b)
for y in range(h):
 for x in range(w):
  if px[x,y]<128: continue
  if y==0 or px[x,y-1]<128:add((x,y),(x+1,y))
  if x==w-1 or px[x+1,y]<128:add((x+1,y),(x+1,y+1))
  if y==h-1 or px[x,y+1]<128:add((x+1,y+1),(x,y+1))
  if x==0 or px[x-1,y]<128:add((x,y+1),(x,y))
loops=[]
while edges:
 start=next(iter(edges));p=start;loop=[]
 while True:
  loop.append(p); opts=edges[p];q=opts.pop()
  if not opts:del edges[p]
  p=q
  if p==start:break
 loops.append(loop)
def area(p):return sum(a[0]*b[1]-b[0]*a[1] for a,b in zip(p,p[1:]+p[:1]))/2
def simplify(p,epsilon=.8):
 if len(p)<3:return p
 a,b=p[0],p[-1];dx=b[0]-a[0];dy=b[1]-a[1];length=math.hypot(dx,dy)
 ds=[abs(dy*(v[0]-a[0])-dx*(v[1]-a[1]))/length if length else math.dist(v,a) for v in p]
 i=max(range(len(ds)),key=ds.__getitem__)
 if ds[i]<=epsilon:return [a,b]
 return simplify(p[:i+1],epsilon)[:-1]+simplify(p[i:],epsilon)
shapes=[]
for p in sorted(loops,key=lambda p:abs(area(p)),reverse=True)[:4]:
 mid=len(p)//2;q=simplify(p[:mid+1])[:-1]+simplify(p[mid:]+p[:1])[:-1]
 shapes.append(q)
result={'width':w,'height':h,'contours':shapes}
(root/'scripts/branding/logo-contours.json').write_text(json.dumps(result,separators=(',',':'))+'\n')
print('Traced original outlines:',[len(p) for p in shapes])
