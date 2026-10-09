const styles=await figma.getLocalTextStylesAsync(),vars=await figma.variables.getLocalVariablesAsync();
const sm=new Map(styles.map(s=>[s.name,s])),vm=new Map(vars.map(v=>[v.name,v]));
const fonts=new Map(styles.map(s=>[JSON.stringify(s.fontName),s.fontName]));
for(const id of ['5:17','5:2','5:293','5:502'])for(const t of (await figma.getNodeByIdAsync(id)).query('TEXT'))for(const seg of t.getStyledTextSegments(['fontName']))fonts.set(JSON.stringify(seg.fontName),seg.fontName);
for(const f of fonts.values())await figma.loadFontAsync(f);
await figma.setCurrentPageAsync(await figma.getNodeByIdAsync('61:701'));
const created=new Set(),mutated=new Set(),links=[];
function track(n){created.add(n.id);if('children'in n)for(const c of n.findAll(()=>true))created.add(c.id);return n;}
function descendants(n){return 'children'in n?n.findAll(()=>true):[];}
function named(n,name){const x=descendants(n).find(c=>c.name===name);if(!x)throw new Error('Missing '+name+' in '+n.id);return x;}
function paint(n,name){n.fills=[figma.variables.setBoundVariableForPaint({type:'SOLID',color:{r:0,g:0,b:0}},'color',vm.get(name))];}
function sp(n,k,v){n[k]=v;if(vm.has('spacing/'+v))n.setBoundVariable(k,vm.get('spacing/'+v));}
function auto(parent,name,dir,w,h){const n=figma.createAutoLayout(dir);parent.appendChild(n);n.name=name;n.resize(w,h);n.fills=[];track(n);return n;}
async function instance(parent,id,name,x,y,w,h){const n=(await figma.getNodeByIdAsync(id)).createInstance();parent.appendChild(n);if(name)n.name=name;if(w!==undefined)n.resize(w,h);if(x!==undefined)n.x=x;if(y!==undefined)n.y=y;track(n);return n;}
async function clone(parent,id,name,x,y,w,h){const n=(await figma.getNodeByIdAsync(id)).clone();parent.appendChild(n);if(name)n.name=name;if(w!==undefined)n.resize(w,h);n.x=x;n.y=y;track(n);return n;}
async function text(parent,label,style='Aegis/Label'){const n=figma.createText();parent.appendChild(n);await n.setTextStyleIdAsync(sm.get(style).id);n.characters=label;n.name=label;n.textAutoResize='WIDTH_AND_HEIGHT';paint(n,'color/text');track(n);return n;}
async function nav(n,dest){await n.setReactionsAsync([{trigger:{type:'ON_CLICK'},actions:[{type:'NODE',destinationId:dest,navigation:'NAVIGATE',transition:{type:'DISSOLVE',easing:{type:'EASE_OUT'},duration:0.15},resetScrollPosition:false}]}]);links.push({id:n.id,to:dest});}
async function back(n){await n.setReactionsAsync([{trigger:{type:'ON_CLICK'},actions:[{type:'BACK'}]}]);links.push({id:n.id,to:'BACK'});}
const tablet=await figma.getNodeByIdAsync('61:702'),tabletOpen=await figma.getNodeByIdAsync('61:703');
for(const root of [tablet,tabletOpen]){
  if(root.children.length)throw new Error('Screen already populated '+root.id);
  const status=await clone(root,'5:2','Status bar',0,0,1200,40);
  const win=await clone(root,'5:17','Settings window',112,64,1072,736);
  const sidebar=named(win,'Settings sidebar');sidebar.resize(224,736);sidebar.layoutSizingHorizontal='FIXED';
  const controls=named(sidebar,'Window controls');controls.resize(136,48);controls.layoutSizingHorizontal='FIXED';controls.layoutSizingVertical='FIXED';
  for(const cell of controls.children){cell.resize(40,48);cell.layoutSizingHorizontal='FIXED';cell.layoutSizingVertical='FIXED';}
  const shell=named(sidebar,'Sidebar content');const line=shell.children.find(c=>c.type==='RECTANGLE'&&c.name==='Divider');
  if(line){line.resize(184,line.height);}
  if(root.id===tablet.id){
    const rail=await clone(root,'5:293','App rail',8,224,88,392);
    await nav(named(rail,'Open app drawer'),tabletOpen.id);
  }else{
    const drawer=await instance(root,'3:142','App drawer',8,56,296,760);
    const header=named(drawer,'Drawer heading');header.resize(248,48);header.layoutSizingHorizontal='FILL';header.layoutSizingVertical='FIXED';
    const close=named(drawer,'Close app drawer');close.resize(48,48);close.layoutSizingHorizontal='FIXED';close.layoutSizingVertical='FIXED';
    await nav(close,tablet.id);await nav(named(drawer,'Einstellungen'),tablet.id);
  }
  root.placeholder=false;mutated.add(root.id);
}
async function phoneBase(root){
  if(root.children.length)throw new Error('Screen already populated '+root.id);
  await instance(root,'60:1007','Status bar',0,0);
}
async function phoneDock(root){
  const dock=await instance(root,'60:733','App rail',8,784);
  await nav(named(dock,'Open app drawer'),'61:706');
  const pinned=named(dock,'Pinned apps');if(root.id!=='61:704')await nav(pinned.children[pinned.children.length-1],'61:704');
  return dock;
}
const index=await figma.getNodeByIdAsync('61:704');await phoneBase(index);
const overview=await instance(index,'60:957','Einstellungen',8,48);
const privacyLink=overview.findAllWithCriteria({types:['TEXT']}).find(n=>n.characters==='Privatsphäre');
await nav(privacyLink.parent,'61:705');await phoneDock(index);index.placeholder=false;mutated.add(index.id);
const phone=await figma.getNodeByIdAsync('61:705');await phoneBase(phone);
const surface=auto(phone,'Settings app','VERTICAL',392,728);surface.x=8;surface.y=48;surface.cornerRadius=24;surface.setBoundVariable('cornerRadius',vm.get('radius/floating'));surface.clipsContent=true;paint(surface,'color/surface');
const toolbar=auto(surface,'Navigation','HORIZONTAL',392,56);toolbar.layoutSizingHorizontal='FILL';toolbar.counterAxisAlignItems='CENTER';sp(toolbar,'paddingLeft',8);sp(toolbar,'paddingRight',16);sp(toolbar,'itemSpacing',8);
const backButton=auto(toolbar,'Back to settings','HORIZONTAL',48,48);backButton.primaryAxisAlignItems='CENTER';backButton.counterAxisAlignItems='CENTER';await instance(backButton,'3:27','Chevron',undefined,undefined,24,24);await nav(backButton,index.id);
const label=await text(toolbar,'Einstellungen');label.layoutSizingHorizontal='FILL';
const scroll=figma.createFrame();surface.appendChild(scroll);scroll.name='Privacy · Vertical scroll';scroll.resize(392,672);scroll.fills=[];scroll.clipsContent=true;scroll.overflowDirection='VERTICAL';scroll.layoutSizingHorizontal='FILL';scroll.layoutSizingVertical='FILL';track(scroll);
await instance(scroll,'60:846','Privacy content',0,0);await phoneDock(phone);phone.placeholder=false;mutated.add(phone.id);
const phoneOpen=await figma.getNodeByIdAsync('61:706');await phoneBase(phoneOpen);
const dismiss=figma.createFrame();phoneOpen.appendChild(dismiss);dismiss.name='Close drawer · Backdrop';dismiss.resize(408,280);dismiss.x=0;dismiss.y=40;dismiss.fills=[];track(dismiss);await back(dismiss);
const drawer=await instance(phoneOpen,'60:757','App drawer',8,320);
await back(named(drawer,'Close app drawer'));await nav(named(drawer,'Einstellungen'),index.id);phoneOpen.placeholder=false;mutated.add(phoneOpen.id);
for(const root of [tablet,tabletOpen,index,phone,phoneOpen])for(const n of descendants(root))created.add(n.id);
return {createdNodeIds:[...created],mutatedNodeIds:[...mutated],pageId:'61:701',screens:[tablet,tabletOpen,index,phone,phoneOpen].map(n=>({id:n.id,name:n.name,width:n.width,height:n.height,children:n.children.map(c=>({id:c.id,name:c.name,x:c.x,y:c.y,width:c.width,height:c.height}))})),links,scroll:{id:scroll.id,viewportHeight:scroll.height,contentHeight:scroll.children[0].height,overflowDirection:scroll.overflowDirection}};
