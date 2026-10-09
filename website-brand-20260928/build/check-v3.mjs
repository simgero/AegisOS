import {chromium} from 'playwright';
import fs from 'node:fs/promises';
const root='/Users/simeongerodetti/AegisOS/website-brand-20260928/build-v3';
const b=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
const p=await b.newPage({viewport:{width:1440,height:1050}});let errors=[];p.on('pageerror',e=>errors.push(e.message));await p.goto('http://127.0.0.1:8767');await p.screenshot({path:root+'/desktop.png'});
await p.locator('#scope-user').click();if(!(await p.locator('#scope-command').innerText()).includes('--scope user'))throw Error('scope');await p.locator('#scope-user').press('ArrowLeft');if(await p.locator('#scope-all').getAttribute('aria-selected')!=='true')throw Error('keyboard');
await p.locator('[data-dialog="contribute"]').click();if(!await p.locator('#dialog').isVisible())throw Error('dialog');await p.keyboard.press('Escape');
await p.locator('#technologie').scrollIntoViewIfNeeded();await p.screenshot({path:root+'/architecture.png'});await p.locator('#fortschritt').scrollIntoViewIfNeeded();await p.screenshot({path:root+'/status.png'});
let overflow=[];for(const width of [390,768,1440]){await p.setViewportSize({width,height:900});await p.goto('http://127.0.0.1:8767');if(await p.evaluate(()=>document.documentElement.scrollWidth>innerWidth))overflow.push(width);if(width===390){await p.screenshot({path:root+'/mobile.png'});await p.locator('.menu').click();if(await p.locator('.menu').getAttribute('aria-expanded')!=='true')throw Error('menu');await p.keyboard.press('Escape');await p.locator('.package-section').scrollIntoViewIfNeeded();await p.screenshot({path:root+'/mobile-package.png'});}}
const assets=await p.evaluate(()=>[...document.images].map(i=>({src:i.getAttribute('src'),ok:i.complete&&i.naturalWidth>0})));let badLinks=[];for(const url of await p.locator('a[download]').evaluateAll(as=>[...new Set(as.map(a=>a.href))])){const r=await p.request.get(url);if(!r.ok())badLinks.push(url);}
console.log(JSON.stringify({errors,overflow,assets,badLinks}));await fs.writeFile(root+'/qa.json',JSON.stringify({errors,overflow,assets,badLinks},null,2));await b.close();
