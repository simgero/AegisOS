await figma.setCurrentPageAsync(await figma.getNodeByIdAsync('2:2'));
const changed = new Set();
function touch(n) { changed.add(n.id); return n; }
const roots = [];
for (const id of ['4:125','31:773','4:18','4:31','4:44']) roots.push(await figma.getNodeByIdAsync(id));
const styles = await figma.getLocalTextStylesAsync();
const fonts = new Map();
for (const root of roots) for (const t of root.query('TEXT')) for (const seg of t.getStyledTextSegments(['fontName'])) fonts.set(JSON.stringify(seg.fontName), seg.fontName);
for (const style of styles) fonts.set(JSON.stringify(style.fontName), style.fontName);
for (const font of fonts.values()) await figma.loadFontAsync(font);
const vars = await figma.variables.getLocalVariablesAsync();
const byName = new Map(vars.map(v => [v.name, v]));
const styleByName = new Map(styles.map(s => [s.name, s]));
function spacing(n, field, value) { n[field] = value; const v = byName.get('spacing/' + value); if (v) n.setBoundVariable(field, v); touch(n); }
function resize(n, w, h) { const sh = n.layoutSizingHorizontal, sv = n.layoutSizingVertical; n.resize(w,h); n.layoutSizingHorizontal=sh; n.layoutSizingVertical=sv; touch(n); }
async function style(n, name) { await n.setTextStyleIdAsync(styleByName.get(name).id); touch(n); }
function fill(n, token) { n.fills=[figma.variables.setBoundVariableForPaint({type:'SOLID',color:{r:0,g:0,b:0}},'color',byName.get(token))];touch(n); }
function child(n, name) { const c = n.children.find(x => x.name === name); if(!c)throw new Error('Missing child '+name+' in '+n.id);return c; }
function noRule(n) { n.strokes=[];touch(n); }
function space(n,h){resize(n,8,h);n.name='Space / '+h;touch(n);}
async function deviceRow(row) {
  noRule(row);spacing(row,'itemSpacing',16);
  const icon=row.children[0];resize(icon,24,24);
  const copy=child(row,'Copy');await style(copy.children[0],'Aegis/Small strong');
  await style(copy.children[1],'Aegis/Body');
  fill(copy.children[0],'color/text');fill(copy.children[1],'color/text-secondary');
}
async function recentRow(row) {
  noRule(row);spacing(row,'itemSpacing',16);
  resize(row.children[0],24,24);
  const label=row.children[1];await style(label,'Aegis/Body');resize(label,160,24);
  const type=row.children[2];await style(type,'Aegis/Body');fill(type,'color/text-secondary');
  const time=row.children[3];await style(time,'Aegis/Body');fill(time,'color/text-secondary');
}
for(const id of ['4:18','4:31']) {
  const n=await figma.getNodeByIdAsync(id);await deviceRow(n);
  n.description='Gerätezugriff mit 24-px-Icon, hervorgehobener Bezeichnung und sekundärer Erklärung. Linienfreie 72-px-Zeile; 16 px Abstand zwischen den Elementen.';
  touch(n);
}
const recentMain=await figma.getNodeByIdAsync('4:44');await recentRow(recentMain);
recentMain.description='Zurückhaltende Verlaufzeile ohne Trennlinie. 24-px-App-Icon, App-Name und Zugriffsart; Zeitpunkt rechts. 48 px Höhe.';
touch(recentMain);
const config = [
  {content:'4:125',header:'4:126',intro:'4:128',gapTop:'4:129',status:'4:130',unusedGap:'4:135',devices:'4:136',gapActions:'4:185',recent:'4:186',gapRecent:'4:218',apps:'4:219'},
  {content:'31:773',header:'31:774',intro:'31:776',gapTop:'31:777',status:'31:778',unusedGap:'31:781',devices:'31:782',gapActions:'31:788',recent:'31:789',gapRecent:'31:797',apps:'31:798'}
];
const results=[];
for (const cfg of config) {
  const n={}; for(const [key,id] of Object.entries(cfg))n[key]=await figma.getNodeByIdAsync(id);
  for(const f of ['paddingTop','paddingRight','paddingBottom','paddingLeft'])spacing(n.content,f,32);
  n.intro.visible=false;touch(n.intro);
  n.status.fills=[];noRule(n.status);
  for(const f of ['paddingTop','paddingRight','paddingBottom','paddingLeft'])spacing(n.status,f,0);
  spacing(n.status,'itemSpacing',8);
  n.status.counterAxisAlignItems='CENTER';touch(n.status);
  resize(n.status,n.header.width,24);
  resize(n.status.children[0],16,16);
  await style(n.status.children[1],'Aegis/Body');fill(n.status.children[1],'color/text-secondary');
  n.header.appendChild(n.status);touch(n.header);touch(n.status);touch(n.content);
  n.status.layoutSizingHorizontal='FILL';n.status.layoutSizingVertical='FIXED';
  n.header.layoutSizingVertical='HUG';spacing(n.header,'itemSpacing',8);
  space(n.gapTop,32);n.unusedGap.visible=false;touch(n.unusedGap);
  for(const row of n.devices.children.filter(c=>c.type==='INSTANCE'))await deviceRow(row);
  space(n.gapActions,8);
  noRule(n.apps);spacing(n.apps,'itemSpacing',16);resize(n.apps.children[0],24,24);
  const appCopy=child(n.apps,'Copy');await style(appCopy.children[0],'Aegis/Small strong');fill(appCopy.children[0],'color/text');
  space(n.gapRecent,32);
  const recentHeading=child(n.recent,'Section heading');
  resize(recentHeading,recentHeading.width,24);
  await style(recentHeading.children[0],'Aegis/Small strong');
  fill(recentHeading.children[0],'color/text');
  resize(recentHeading.children[0],recentHeading.children[0].width,24);
  await style(recentHeading.children[1],'Aegis/Label');
  recentHeading.children[1].textDecoration='NONE';touch(recentHeading.children[1]);
  fill(recentHeading.children[1],'color/text-secondary');
  const recentSpacer=n.recent.children.find(c=>c.type==='FRAME'&&c.children.length===0);
  space(recentSpacer,16);
  for(const row of n.recent.children.filter(c=>c.type==='INSTANCE'))await recentRow(row);
  const order=[n.header,n.gapTop,n.unusedGap,n.devices,n.gapActions,n.apps,n.gapRecent,n.recent];
  order.forEach((c,i)=>{n.content.insertChild(i,c);touch(c);});
  touch(n.content);
  results.push({id:n.content.id,width:n.content.width,height:n.content.height,sections:n.content.children.filter(c=>c.visible).map(c=>({id:c.id,name:c.name,x:c.x,y:c.y,width:c.width,height:c.height})),visibleRules:n.content.query('FRAME, INSTANCE').toArray().filter(c=>c.visible&&c.strokes.length>0).map(c=>c.id)});
}
return {mutatedNodeIds:[...changed],createdNodeIds:[],content:results};
