await figma.setCurrentPageAsync(await figma.getNodeByIdAsync('2:2'));
const styles=await figma.getLocalTextStylesAsync();
const vars=await figma.variables.getLocalVariablesAsync();
const sv=new Map(styles.map(s=>[s.name,s]));
const vv=new Map(vars.map(v=>[v.name,v]));
const fonts=new Map(styles.map(s=>[JSON.stringify(s.fontName),s.fontName]));
for(const id of ['3:141','3:142','4:44','4:125','4:74','4:121'])for(const t of (await figma.getNodeByIdAsync(id)).query('TEXT'))for(const s of t.getStyledTextSegments(['fontName']))fonts.set(JSON.stringify(s.fontName),s.fontName);
for(const f of fonts.values())await figma.loadFontAsync(f);
const created=new Set(),removed=new Set(),roots=[];
function all(n){return [n,...('children'in n?n.findAll(()=>true):[])];}
function track(n){for(const x of all(n))created.add(x.id);return n;}
function space(n,k,v){n[k]=v;if(vv.has('spacing/'+v))n.setBoundVariable(k,vv.get('spacing/'+v));}
function size(n,w,h){n.resize(w,h);}
function color(n,name){n.fills=[figma.variables.setBoundVariableForPaint({type:'SOLID',color:{r:0,g:0,b:0}},'color',vv.get(name))];}
function named(n,name){const x=n.children.find(c=>c.name===name);if(!x)throw new Error('Missing '+name+' in '+n.name);return x;}
async function txt(parent,label,styleName,colorName='color/text'){const n=figma.createText();parent.appendChild(n);await n.setTextStyleIdAsync(sv.get(styleName).id);n.characters=label;n.name=label;n.textAutoResize='WIDTH_AND_HEIGHT';color(n,colorName);track(n);return n;}
function frame(parent,name,dir,w,h){const n=figma.createAutoLayout(dir);parent.appendChild(n);n.name=name;n.fills=[];size(n,w,h);track(n);return n;}
function comp(name,x,y,w,h,dir='VERTICAL'){if(figma.currentPage.children.some(n=>n.name===name))throw new Error('Already exists '+name);const n=figma.createComponent();n.name=name;n.layoutMode=dir;n.resize(w,h);n.x=x;n.y=y;n.fills=[];n.primaryAxisSizingMode='FIXED';n.counterAxisSizingMode='FIXED';roots.push(n);track(n);return n;}
async function copyComponent(id,name,x,y){if(figma.currentPage.children.some(n=>n.name===name))throw new Error('Already exists '+name);const n=(await figma.getNodeByIdAsync(id)).clone();n.name=name;n.x=x;n.y=y;roots.push(n);track(n);return n;}
function fillChild(n){n.layoutSizingHorizontal='FILL';}
async function icon(parent,id,w=24){const n=(await figma.getNodeByIdAsync(id)).createInstance();parent.appendChild(n);n.resize(w,w);track(n);return n;}
const dock=await copyComponent('3:141','App rail / Bottom',3600,200);
dock.layoutMode='HORIZONTAL';dock.resize(392,88);dock.layoutSizingHorizontal='FIXED';dock.layoutSizingVertical='FIXED';
const pin=named(dock,'Pinned apps');pin.layoutMode='HORIZONTAL';pin.resize(272,56);pin.layoutSizingHorizontal='FIXED';pin.layoutSizingVertical='FIXED';
const divider=named(dock,'Divider');divider.resize(1,56);divider.x=88;divider.y=16;
dock.description='App-Leiste an der kurzen unteren Bildschirmkante im Hochformat. Identische 56-px-App-Träger wie am Desktop; 392 × 88 px.';
const drawer=await copyComponent('3:142','App drawer / Bottom',4096,200);
drawer.resize(392,552);drawer.layoutSizingHorizontal='FIXED';drawer.layoutSizingVertical='HUG';
const dh=named(drawer,'Drawer heading');dh.resize(344,48);fillChild(dh);dh.layoutSizingVertical='FIXED';
const close=named(dh,'Close app drawer');close.resize(48,48);close.layoutSizingHorizontal='FIXED';close.layoutSizingVertical='FIXED';
const down=close.children[0];down.rotation=90;
const spacers=drawer.children.filter(n=>n.name==='Space / 24');
spacers[0].resize(8,16);spacers[0].name='Space / 16';
const grid=named(drawer,'Apps grid');const oldRows=[...grid.children];const items=oldRows.flatMap(r=>[...r.children]);
space(grid,'itemSpacing',16);grid.resize(344,368);fillChild(grid);grid.layoutSizingVertical='HUG';
for(let i=0;i<3;i++){const row=frame(grid,'App row '+(i+1),'HORIZONTAL',344,112);space(row,'itemSpacing',16);fillChild(row);for(const item of items.slice(i*3,i*3+3)){row.appendChild(item);item.resize(104,112);item.layoutSizingHorizontal='FIXED';item.layoutSizingVertical='FIXED';const label=named(item,'App label');label.resize(104,24);label.layoutSizingHorizontal='FILL';const dot=item.children.find(n=>n.name==='Active app');if(dot){dot.x=50;}}}
for(const row of oldRows){removed.add(row.id);row.remove();}
drawer.description='App-Drawer an der kurzen unteren Bildschirmkante. Drei Spalten, 80-px-App-Träger, lesbare Labels; 48-px-Schließenfläche mit Chevron nach unten.';
const recent=await copyComponent('4:44','Recent access row / Mobile',3600,400);
recent.resize(360,64);
const appName=recent.children[1],permission=recent.children[2],when=recent.children[3];
const copy=frame(recent,'Copy','VERTICAL',208,48);recent.insertChild(1,copy);copy.appendChild(appName);copy.appendChild(permission);
copy.layoutSizingHorizontal='FILL';copy.layoutSizingVertical='HUG';
for(const t of [appName,permission]){t.textAutoResize='HEIGHT';t.resize(208,24);t.layoutSizingHorizontal='FILL';t.layoutSizingVertical='HUG';}
recent.description='Mobiler Verlauf: App und Berechtigung untereinander, Zeitpunkt rechts. 24-px-Icon, 64-px-Zeile, keine Trennlinien.';
const privacy=comp('Privacy content / Mobile',4592,200,392,760);
privacy.layoutSizingVertical='HUG';
const body=(await figma.getNodeByIdAsync('4:125')).clone();privacy.appendChild(body);track(body);body.resize(392,760);body.layoutSizingHorizontal='FILL';body.layoutSizingVertical='HUG';
space(body,'paddingTop',16);space(body,'paddingRight',16);space(body,'paddingBottom',24);space(body,'paddingLeft',16);
const heading=named(body,'Page heading');const status=named(heading,'Privacy status');const st=status.children.find(n=>n.type==='TEXT');
st.textAutoResize='HEIGHT';st.resize(336,48);st.layoutSizingHorizontal='FILL';st.layoutSizingVertical='HUG';status.layoutSizingVertical='HUG';
const device=named(body,'Gerätezugriff');const location=device.children.filter(n=>n.type==='INSTANCE')[2];named(location,'Copy').children[1].characters='Pro App festlegen';
const history=named(body,'Letzte Zugriffe');history.layoutSizingVertical='HUG';
for(const row of history.children.filter(n=>n.type==='INSTANCE')){const label=row.children[1].characters;const access=row.children[2].characters;const time=row.children[3].characters;const oldIcon=row.children[0].mainComponent;row.swapComponent(recent);row.resize(360,64);row.layoutSizingHorizontal='FILL';row.children[0].swapComponent(oldIcon);row.children[0].resize(24,24);const cp=named(row,'Copy');cp.children[0].characters=label;cp.children[1].characters=access;row.children.find(n=>n.type==='TEXT').characters=time;track(row);}
privacy.description='Schmale Privacy-Ansicht mit denselben Komponenten und derselben Hierarchie wie am Desktop. 392 px breit; zweizeiliger Status, kompakte Standort-Erklärung und gestapelter Verlauf. In einem vertikal scrollenden Viewport verwenden.';
const overview=comp('Settings overview / Mobile',5088,200,392,728);
const sb=await figma.getNodeByIdAsync('4:67');overview.fills=sb.fills;overview.effects=sb.effects;overview.cornerRadius=24;overview.setBoundVariable('cornerRadius',vv.get('radius/floating'));overview.clipsContent=true;
for(const p of ['paddingLeft','paddingRight'])space(overview,p,16);
for(const p of ['paddingTop','paddingBottom'])space(overview,p,24);
const title=await txt(overview,'Einstellungen','Aegis/Heading');
const gap1=frame(overview,'Space / 24','VERTICAL',8,24);
const search=(await figma.getNodeByIdAsync('3:117')).createInstance();overview.appendChild(search);search.resize(360,48);fillChild(search);track(search);
frame(overview,'Space / 24','VERTICAL',8,24);
const nav=(await figma.getNodeByIdAsync('4:74')).clone();overview.appendChild(nav);nav.resize(360,328);fillChild(nav);track(nav);
for(const row of nav.children){const label=row.query('TEXT').first().characters;const ic=row.children.find(n=>n.type==='INSTANCE').mainComponent;row.swapComponent(await figma.getNodeByIdAsync('3:106'));row.resize(360,48);fillChild(row);row.query('TEXT').first().characters=label;row.children.find(n=>n.type==='INSTANCE').swapComponent(ic);track(row);}
const flex=frame(overview,'Flexible space','VERTICAL',8,32);flex.layoutSizingVertical='FILL';
const account=(await figma.getNodeByIdAsync('4:121')).clone();overview.appendChild(account);account.resize(360,48);fillChild(account);track(account);
overview.description='Mobile Einstellungsübersicht mit Glasfläche, Suche, sechs Navigationszielen und Account. 48-px-Navigationszeilen. Privatsphäre führt zur Detailansicht.';
const statusbar=comp('Status bar / Mobile',3600,600,408,40,'HORIZONTAL');
space(statusbar,'paddingLeft',16);space(statusbar,'paddingRight',16);space(statusbar,'itemSpacing',16);statusbar.counterAxisAlignItems='CENTER';
const time=await txt(statusbar,'09:41','Aegis/Small strong');time.layoutSizingHorizontal='FILL';
await icon(statusbar,'3:9',16);await icon(statusbar,'3:56',24);
statusbar.description='Schmale Systemleiste: Uhrzeit, WLAN und Batterie. 40 px hoch, variable Breite.';
for(const root of roots){for(const n of all(root)){if('reactions'in n&&n.reactions.length)await n.setReactionsAsync([]);}track(root);}
return {createdNodeIds:[...created],removedNodeIds:[...removed],components:{dock:dock.id,drawer:drawer.id,recent:recent.id,privacy:privacy.id,overview:overview.id,statusbar:statusbar.id},bounds:roots.map(n=>({id:n.id,name:n.name,width:n.width,height:n.height})),privacySections:body.children.filter(n=>n.visible).map(n=>({name:n.name,y:n.y,height:n.height}))};
