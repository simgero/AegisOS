import fs from 'node:fs/promises';import sharp from 'sharp';
const dir='/Users/simeongerodetti/AegisOS/website-brand-20260928/output/brand-assets';
for(const f of await fs.readdir(dir)){if(f.endsWith('.svg'))await sharp(dir+'/'+f).resize(f.includes('symbol')?1024:1860).png().toFile(dir+'/'+f.replace('.svg','.png'));}
