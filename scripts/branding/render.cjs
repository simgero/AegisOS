// Render the approved code-native boot design at deterministic timestamps.
// Requires Playwright, Chrome and Arial; Android builds consume the packaged ZIP.
const { chromium } = require('playwright');
const fs = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
(async () => {
 const output=path.resolve(__dirname,'../../out/branding/production');
 await fs.mkdir(output,{recursive:true});
 const browser=await chromium.launch({headless:true,channel:'chrome'});
 try {
  // Supersample moving SVG contours; package.py downsamples once to device pixels.
  const page=await browser.newPage({viewport:{width:720,height:720},deviceScaleFactor:3});
  const errors=[];page.on('pageerror',e=>errors.push(e.message));
  await page.goto(pathToFileURL(path.join(__dirname,'boot-preview.html')).href);
  await page.addStyleTag({content:'.replay,.controls{display:none!important}'});
  await page.evaluate(async()=>{await document.fonts.ready;bootPreview.seek(0)});
  // A square repeats after a quarter turn: retain the 16.8 s full-turn speed.
  const fps=30,introFrames=158,loopFrames=126;
  for(const [part,count] of [['part0',introFrames],['part1',loopFrames]]){
   const dir=path.join(output,part);await fs.mkdir(dir,{recursive:true});
   for(const file of await fs.readdir(dir))if(/^\d{4}\.png$/.test(file)&&Number(file.slice(0,4))>=count)await fs.unlink(path.join(dir,file));
   for(let n=0;n<count;n++){
    const ms=(part==='part0'?n:introFrames+n)*1000/fps;
    await page.evaluate(ms=>bootPreview.seek(ms),ms);
    await page.screenshot({path:path.join(dir,`${String(n).padStart(4,'0')}.png`)});
   }
   console.log(`${part}: ${count} frames rendered`);
  }
  if(errors.length)throw Error(errors.join('\n'));
  await fs.writeFile(path.join(output,'manifest.json'),JSON.stringify({width:720,height:720,fps,introFrames,loopFrames,loopSeconds:4.2},null,2)+'\n');
 }finally{await browser.close()}
})().catch(error=>{console.error(error);process.exitCode=1});
