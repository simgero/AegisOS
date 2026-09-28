"""Build a static vector proof from original alpha contours and sampled colours."""
from pathlib import Path
from PIL import Image,ImageDraw
import numpy as np
import json
root=Path(__file__).resolve().parents[2]
original=Image.open(root/'device/aegis/qemu_arm64/branding/logo.png').convert('RGBA')
rgba=np.asarray(original);h,w=rgba.shape[:2]
contours=json.loads((root/'scripts/branding/logo-contours.json').read_text())['contours']
svg=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" role="img" aria-label="Aegis Zeichen – statische Vektorfassung">','<defs>']
paths=[]
for index,contour in enumerate(contours):
 p=np.asarray(contour,dtype=float);delta=np.roll(p,-1,axis=0)-p
 lengths=np.linalg.norm(delta,axis=1);cum=np.r_[0,np.cumsum(lengths)]
 count=round(cum[-1]/3);locations=np.arange(count)*cum[-1]/count
 closed=np.vstack([p,p[0]])
 uniform=np.column_stack([np.interp(locations,cum,closed[:,k]) for k in range(2)])
 # Remove pixel stair-steps, without changing the original shape by hand.
 weights=np.exp(-np.arange(-3,4)**2/(2*1.3**2));weights/=weights.sum()
 smoothed=sum(weight*np.roll(uniform,shift,axis=0) for shift,weight in zip(range(-3,4),weights))
 points=smoothed[::3]
 d=f'M{points[0,0]:.3f},{points[0,1]:.3f}'
 for i,b in enumerate(points):
  a=points[(i-1)%len(points)];c=points[(i+1)%len(points)];e=points[(i+2)%len(points)]
  c1=b+(c-a)/6;c2=c-(e-b)/6
  d+=f'C{c1[0]:.3f},{c1[1]:.3f} {c2[0]:.3f},{c2[1]:.3f} {c[0]:.3f},{c[1]:.3f}'
 paths.append(f'<path fill="url(#tone{index})" d="{d}Z"/>')
 maskImage=Image.new('1',(w,h));ImageDraw.Draw(maskImage).polygon([tuple(x) for x in contour],fill=1)
 mask=np.asarray(maskImage).copy()&(rgba[:,:,3]>250)
 for dy,dx in [(0,8),(0,-8),(8,0),(-8,0)]:mask&=np.roll(np.asarray(maskImage),(dy,dx),(0,1))
 y,x=np.where(mask);step=max(1,len(x)//20000);x=x[::step];y=y[::step]
 X=np.column_stack([np.ones(len(x)),x/w,y/h]);colors=rgba[y,x,:3].astype(float)
 coeff=np.linalg.lstsq(X,colors,rcond=None)[0]
 direction=coeff[1:].mean(axis=1);norm=np.linalg.norm(direction)
 direction=direction/norm if norm>.001 else np.array([0.,1.])
 center=np.array([x.mean()/w,y.mean()/h]);xy=np.column_stack([x/w,y/h]);t=(xy-center)@direction
 start=center+direction*t.min();end=center+direction*t.max()
 def color(v):return '#'+''.join(f'{int(round(c)):02x}' for c in np.clip(np.r_[1,v]@coeff,0,255))
 svg.append(f'<linearGradient id="tone{index}" gradientUnits="userSpaceOnUse" x1="{start[0]*w:.3f}" y1="{start[1]*h:.3f}" x2="{end[0]*w:.3f}" y2="{end[1]*h:.3f}"><stop stop-color="{color(start)}"/><stop offset="1" stop-color="{color(end)}"/></linearGradient>')
svg+=['</defs>',*paths,'</svg>']
import re
result=re.sub(r'<defs>.*?</defs>\n?', '', '\n'.join(svg), flags=re.S)
for i in range(4): result=result.replace(f'url(#tone{i})', '#C4F15A' if i==3 else '#171C23')
(root/'scripts/branding/logo-static.svg').write_text(result+'\n')
print('Created static SVG: four traced paths; colours fitted from original; no animation.')
