const page=await figma.getNodeByIdAsync('2:2');await figma.setCurrentPageAsync(page);
const changed=new Set([page.id]);const mark=n=>{changed.add(n.id);return n;};
const fonts=new Map();for(const t of page.findAllWithCriteria({types:['TEXT']}))for(const seg of t.getStyledTextSegments(['fontName']))fonts.set(JSON.stringify(seg.fontName),seg.fontName);await Promise.all([...fonts.values()].map(f=>figma.loadFontAsync(f)));
const allVars=await figma.variables.getLocalVariablesAsync();const vars=Object.fromEntries(allVars.map(v=>[v.name,v]));const cols=await figma.variables.getLocalVariableCollectionsAsync();const prim=cols.find(c=>c.name==='Aegis · Primitives'),ui=cols.find(c=>c.name==='Aegis · UI');
if(!vars['scale/2']){const v=figma.variables.createVariable('scale/2',prim,'FLOAT');v.scopes=[];v.setValueForMode(prim.defaultModeId,2);v.setVariableCodeSyntax('WEB','var(--aegis-scale-2)');vars[v.name]=v;}
if(!vars['spacing/2']){const v=figma.variables.createVariable('spacing/2',ui,'FLOAT');v.scopes=['GAP'];v.setValueForMode(ui.defaultModeId,{type:'VARIABLE_ALIAS',id:vars['scale/2'].id});v.setVariableCodeSyntax('WEB','var(--aegis-space-2)');vars[v.name]=v;}
vars['radius/window'].setValueForMode(ui.defaultModeId,{type:'VARIABLE_ALIAS',id:vars['scale/16'].id});vars['radius/pill'].setValueForMode(ui.defaultModeId,{type:'VARIABLE_ALIAS',id:vars['scale/16'].id});
if(!vars['radius/avatar']){const v=figma.variables.createVariable('radius/avatar',ui,'FLOAT');v.scopes=['CORNER_RADIUS'];v.setValueForMode(ui.defaultModeId,{type:'VARIABLE_ALIAS',id:vars['scale/24'].id});v.setVariableCodeSyntax('WEB','var(--aegis-radius-avatar)');vars[v.name]=v;}
const valid=v=>v===0||v===2||v===4||Math.abs(v/8-Math.round(v/8))<0.001;
const snap=v=>v<=4?(v===0?0:v<=2?2:4):Math.max(8,Math.round(v/8)*8);
const local=page.findAll().filter(n=>!n.id.startsWith('I'));
function insideIcon(n){let p=n;while(p&&p.type!=='PAGE'){if(p.name.startsWith('Icon/'))return true;p=p.parent;}return false;}
function sz(n,w,h){mark(n);const a=n.layoutSizingHorizontal,b=n.layoutSizingVertical;n.resize(w,h);if(a==='FILL'||a==='HUG')n.layoutSizingHorizontal=a;if(b==='FILL'||b==='HUG')n.layoutSizingVertical=b;return n;}
function fixed(n,w,h){mark(n);n.resize(w,h);return n;}
function spacing(n,key,v){mark(n);n.setBoundVariable(key,vars['spacing/'+v]);}
function padding(n,t,r=t,b=t,l=r){spacing(n,'paddingTop',t);spacing(n,'paddingRight',r);spacing(n,'paddingBottom',b);spacing(n,'paddingLeft',l);}
function gap(n,v){spacing(n,'itemSpacing',v);}
function rad(n,k){mark(n);for(const p of ['topLeftRadius','topRightRadius','bottomLeftRadius','bottomRightRadius'])n.setBoundVariable(p,vars['radius/'+k]);}
function find(root,name){const n=root.findOne(x=>x.name===name);if(!n)throw new Error('Missing '+name+' in '+root.name);return n;}
const byId=async id=>await figma.getNodeByIdAsync(id);
// Normalize explicit layout properties. Imported vector contents stay untouched.
for(const n of local){if(insideIcon(n)||n.type==='VECTOR'||n.type==='TEXT')continue;
if('layoutMode'in n&&n.layoutMode!=='NONE'){for(const p of ['paddingTop','paddingRight','paddingBottom','paddingLeft','itemSpacing'])spacing(n,p,snap(n[p]));n.strokesIncludedInLayout=false;}
if('topLeftRadius'in n){for(const p of ['topLeftRadius','topRightRadius','bottomLeftRadius','bottomRightRadius'])if(typeof n[p]==='number'&&!valid(n[p])){mark(n);n.setBoundVariable(p,null);n[p]=snap(n[p]);}}
if('strokes'in n&&n.strokes.length){mark(n);for(const p of ['strokeTopWeight','strokeRightWeight','strokeBottomWeight','strokeLeftWeight'])if(p in n&&n[p]>0)n[p]=2;}
if(n.type==='RECTANGLE'&&n.name==='Divider')sz(n,snap(n.width),2);
if((n.type==='FRAME'||n.type==='COMPONENT'||n.type==='COMPONENT_SET'||n.type==='INSTANCE'||n.type==='ELLIPSE')&&(!valid(n.width)||!valid(n.height))){const w=n.layoutSizingHorizontal==='FIXED'?snap(n.width):n.width,h=n.layoutSizingVertical==='FIXED'?snap(n.height):n.height;sz(n,w,h);}
if(n.parent.type==='PAGE'||n.parent.type==='COMPONENT_SET'||n.layoutPositioning==='ABSOLUTE'){mark(n);n.x=Math.round(n.x/8)*8;n.y=Math.round(n.y/8)*8;}}
// Typography uses grid-aligned line boxes. Natural glyph advances remain untouched.
const scale={'Aegis/Heading':[32,40],'Aegis/Section':[24,32],'Aegis/Body':[16,24],'Aegis/Label':[16,24],'Aegis/Small':[16,24],'Aegis/Small strong':[16,24],'Aegis/Drawer title':[24,32]};
const styles=await figma.getLocalTextStylesAsync();for(const st of styles){if(scale[st.name]){st.fontSize=scale[st.name][0];st.lineHeight={unit:'PIXELS',value:scale[st.name][1]};}}
for(const t of local.filter(n=>n.type==='TEXT')){mark(t);const st=styles.find(s=>s.id===t.textStyleId);if(st&&scale[st.name]){t.fontSize=scale[st.name][0];t.lineHeight={unit:'PIXELS',value:scale[st.name][1]};}if(t.textAutoResize!=='WIDTH_AND_HEIGHT')sz(t,snap(t.width),snap(t.height));}
// Core reusable atoms.
const tileRail=await byId('3:97'),tileDrawer=await byId('3:101');fixed(tileRail,56,56);fixed(tileDrawer,80,80);fixed(tileRail.children[0],24,24);fixed(tileDrawer.children[0],32,32);
for(const id of ['3:106','3:111']){const n=await byId(id);fixed(n,256,48);padding(n,8,16);gap(n,16);fixed(n.children[0],24,24);}
const search=await byId('3:117');fixed(search,240,48);padding(search,8,16);gap(search,16);fixed(search.children[0],24,24);search.strokeWeight=2;
for(const id of ['3:122','3:124']){const n=await byId(id);fixed(n,56,32);padding(n,4);rad(n,'pill');fixed(n.children[0],24,24);}
const controls=await byId('3:127');fixed(controls,144,48);for(const box of controls.children){fixed(box,48,48);fixed(box.children[0],24,24);}
const access=await byId('4:43');for(const row of access.children){fixed(row,696,72);padding(row,8);gap(row,24);fixed(row.children[0],32,32);fixed(row.children[row.children.length-1],24,24);const copy=row.children[1];sz(copy,496,48);copy.layoutSizingHorizontal='FILL';const control=row.children[2];if(control.type==='TEXT'){control.textAutoResize='HEIGHT';fixed(control,56,24);control.textAlignHorizontal='RIGHT';}else fixed(control,56,32);}
const recent=await byId('4:44');fixed(recent,696,48);padding(recent,8);gap(recent,24);fixed(recent.children[0],32,32);for(const [i,w] of [[1,200],[2,296],[3,80]]){const t=recent.children[i];t.textAutoResize='HEIGHT';fixed(t,w,24);if(i===2)t.layoutSizingHorizontal='FILL';if(i===3)t.textAlignHorizontal='RIGHT';}
// Window composition: 48 + 736 = 784. Inner content sums to exactly 736.
const windows=await byId('10:727');
for(const win of windows.children){const compact=win.name.includes('Compact'),ww=compact?1000:1048,sw=compact?272:288,cw=ww-sw-64;fixed(win,ww,784);rad(win,'window');const titlebar=find(win,'Title bar');sz(titlebar,ww,48);padding(titlebar,0,8,0,24);const title=titlebar.children[0];title.textAutoResize='HEIGHT';sz(title,ww-176,24);title.layoutSizingHorizontal='FILL';fixed(titlebar.children[1],144,48);
const body=find(win,'Window body');sz(body,ww,736);const side=find(win,'Settings sidebar');fixed(side,sw,736);padding(side,24,16,16,16);const searchInstance=side.children.find(n=>n.type==='INSTANCE'&&n.name==='Search field');fixed(searchInstance,sw-48,48);const list=find(side,'Navigation');fixed(list,sw-32,328);gap(list,8);for(const n of list.children)fixed(n,sw-32,48);
const sideDivider=side.children.find(n=>n.name==='Divider');sideDivider.layoutPositioning='ABSOLUTE';sideDivider.x=24;sideDivider.y=side.height-88;fixed(sideDivider,sw-48,2);
const user=find(side,'Account');fixed(user,sw-48,48);gap(user,16);const avatar=find(user,'Avatar');fixed(avatar,48,48);rad(avatar,'avatar');
const content=find(win,'Privacy content');sz(content,cw+64,736);padding(content,24,32,8,32);const heading=find(content,'Page heading');sz(heading,cw,72);gap(heading,8);
const spacers=content.children.filter(n=>n.name.startsWith('Space /'));for(const n of spacers){sz(n,8,n.height===24?24:16);n.name='Space / '+n.height;}
const banner=find(content,'Privacy status');sz(banner,cw,56);padding(banner,16);gap(banner,16);fixed(banner.children[0],24,24);
const device=find(content,'Gerätezugriff');sz(device,cw,256);const deviceSpace=device.children.find(n=>n.name.startsWith('Space /'));sz(deviceSpace,8,8);for(const n of device.children.filter(n=>n.type==='INSTANCE')){sz(n,cw,72);gap(n,24);}
const history=find(content,'Letzte Zugriffe');sz(history,cw,184);const header=find(history,'Section heading');sz(header,cw,32);const htitle=header.children[0];htitle.textAutoResize='HEIGHT';sz(htitle,cw-112,32);htitle.layoutSizingHorizontal='FILL';const link=header.children[1];link.textAutoResize='HEIGHT';fixed(link,112,24);link.textAlignHorizontal='RIGHT';const hs=history.children.find(n=>n.name.startsWith('Space /'));sz(hs,8,8);for(const n of history.children.filter(n=>n.type==='INSTANCE'))sz(n,cw,48);
const footer=find(content,'App permissions');sz(footer,cw,64);padding(footer,8);gap(footer,24);fixed(footer.children[0],32,32);sz(footer.children[1],cw-120,48);fixed(footer.children[2],24,24);
for(const n of win.findAll()){if(n.id.startsWith('I')||n.type==='TEXT'||insideIcon(n))continue;if(n.type==='FRAME'&&n.name.startsWith('Space /'))sz(n,8,snap(n.height));}}
// Compact launcher rail.
const rail=await byId('3:141');fixed(rail,96,384);padding(rail,16,24,16,16);const launcher=find(rail,'Open app drawer');fixed(launcher,56,56);const spaces=rail.children.filter(n=>n.name.startsWith('Space /'));for(const n of spaces){fixed(n,8,0.01);n.visible=false;mark(n);}const railDivider=find(rail,'Divider');railDivider.layoutPositioning='ABSOLUTE';railDivider.x=16;railDivider.y=80;fixed(railDivider,56,2);
gap(rail,24);const favorites=find(rail,'Pinned apps');fixed(favorites,56,272);gap(favorites,16);for(const n of favorites.children)fixed(n,56,56);const active=rail.children.find(n=>n.name==='Active app');fixed(active,4,4);active.x=80;active.y=338;const edge=find(rail,'Expand drawer');fixed(edge,16,32);edge.x=80;edge.y=184;
// Drawer: 24px padding, 112px app cells and 24px between rows/columns.
const drawer=await byId('3:142');fixed(drawer,296,760);padding(drawer,24);const dh=find(drawer,'Drawer heading');fixed(dh,248,32);const dtitle=dh.children[0];dtitle.textAutoResize='HEIGHT';sz(dtitle,216,32);dtitle.layoutSizingHorizontal='FILL';fixed(dh.children[1],32,32);fixed(dh.children[1].children[0],24,24);const ds=drawer.children.find(n=>n.type==='INSTANCE'&&n.name==='Search field');fixed(ds,248,48);for(const n of drawer.children.filter(n=>n.name.startsWith('Space /'))){fixed(n,8,24);n.name='Space / 24';}
const grid=find(drawer,'Apps grid');fixed(grid,248,520);gap(grid,24);for(const row of grid.children){fixed(row,248,112);gap(row,24);row.clipsContent=false;for(const item of row.children){fixed(item,112,112);gap(item,8);item.clipsContent=false;fixed(item.children.find(n=>n.type==='INSTANCE'),80,80);const label=find(item,'App label');fixed(label,112,24);const labelText=label.children.find(n=>n.type==='TEXT');labelText.textAutoResize='HEIGHT';fixed(labelText,112,24);labelText.textAlignHorizontal='CENTER';if(item.name==='Einstellungen'){const dot=item.children.find(n=>n.name==='Active app');dot.layoutPositioning='ABSOLUTE';fixed(dot,4,4);dot.x=54;dot.y=82;}}}
// Status bar.
const status=await byId('3:143');fixed(status,1584,40);padding(status,0,24);const stats=find(status,'System status');fixed(stats,216,24);gap(stats,24);for(const n of stats.children){if(n.type==='INSTANCE')fixed(n,24,24);else{n.textAutoResize='HEIGHT';fixed(n,48,24);}}const brand=status.children[0];brand.textAutoResize='HEIGHT';sz(brand,1320,24);brand.layoutSizingHorizontal='FILL';
// Consistent page-level component placement and review frame.
let iconIndex=0;for(const n of page.children.filter(n=>n.type==='COMPONENT'&&n.name.startsWith('Icon/'))){mark(n);n.x=200+(iconIndex%10)*96;n.y=200+Math.floor(iconIndex/10)*80;iconIndex++;}
for(const set of page.children.filter(n=>n.type==='COMPONENT_SET')){mark(set);set.cornerRadius=8;if(set.id==='10:727'){set.resize(2120,832);set.x=200;set.y=944;set.children[0].x=24;set.children[0].y=24;set.children[1].x=1096;set.children[1].y=24;for(const n of set.children)mark(n);}else{let x=16;for(const n of set.children){mark(n);n.x=x;n.y=16;x+=n.width+24;}set.resize(snap(x-8),snap(Math.max(...set.children.map(n=>n.height))+32));}}
const review=await byId('6:611');fixed(review,1184,352);padding(review,24,32);gap(review,24);const examples=find(review,'Component examples');gap(examples,48);for(const col of examples.children){sz(col,snap(col.width),224);gap(col,24);}const navCol=examples.children.find(n=>n.name.startsWith('Navigation'));navCol.name='Navigation · 48';const navTitle=navCol.children.find(n=>n.type==='TEXT');navTitle.characters='Navigation · 48';mark(navTitle);for(const n of navCol.children.filter(n=>n.type==='INSTANCE'))fixed(n,256,48);
const appSet=await byId('3:105');appSet.description='8px grid. Rail: 56px carrier with 24px glyph. Drawer: 80px carrier with 32px glyph. White square with 16px corners.';
windows.description='8px grid. Standard1048×784, compact1000×784. Titlebar48, body736; sidebar288/272. Native text and Heroicons. Intrinsic glyph widths and vector paths are excluded from layout grid snapping.';
// Effects also use scale values.
for(const st of await figma.getLocalEffectStylesAsync()){st.effects=st.effects.map(e=>e.type==='DROP_SHADOW'?{...e,offset:{x:snap(e.offset.x),y:snap(e.offset.y)},radius:snap(e.radius),spread:snap(e.spread)}:e);}
// Rebind stale spacing aliases, then remove only the superseded tokens.
const replacements={'spacing/12':'spacing/16','spacing/20':'spacing/24','spacing/28':'spacing/32'};
for(const n of page.findAll()){if(n.id.startsWith('I')||insideIcon(n))continue;if('layoutMode'in n&&n.layoutMode!=='NONE')for(const p of ['paddingTop','paddingRight','paddingBottom','paddingLeft','itemSpacing']){const v=snap(n[p]);if(vars['spacing/'+v])spacing(n,p,v);}}
for(const name of ['spacing/12','spacing/20','spacing/28','scale/12','scale/20','scale/28','scale/999'])if(vars[name])vars[name].remove();
return {mutatedNodeIds:[...changed],styleIds:styles.map(s=>s.id),variables:(await figma.variables.getLocalVariablesAsync('FLOAT')).map(v=>({id:v.id,name:v.name,values:v.valuesByMode})),windows:windows.children.map(n=>({id:n.id,name:n.name,width:n.width,height:n.height})),rail:{width:rail.width,height:rail.height},drawer:{width:drawer.width,height:drawer.height},policy:'Fixed layout values: 0,2,4 and multiples of8. Text advances, centered optical positions and internal SVG paths are content-derived.'};
