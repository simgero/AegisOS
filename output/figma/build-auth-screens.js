
await figma.setCurrentPageAsync(await figma.getNodeByIdAsync(DEVICE.page));
const sm=new Map((await figma.getLocalTextStylesAsync()).map(s=>[s.name,s]));
const vm=new Map((await figma.variables.getLocalVariablesAsync()).map(v=>[v.name,v]));
const em=new Map((await figma.getLocalEffectStylesAsync()).map(e=>[e.name,e]));
for(const s of sm.values())await figma.loadFontAsync(s.fontName);
const ids=Object.fromEntries(FRAMES.map(f=>[f.key,f.id])),created=new Set(),mutated=new Set(),links=[],panels=[];
const cc={};for(const [k,id]of Object.entries(COMPS))cc[k]=await figma.getNodeByIdAsync(id);
const refs={};for(const id of ["3:143","60:1007","70:1004","3:15","3:18","3:6","3:24","3:27","3:141","60:733"])refs[id]=await figma.getNodeByIdAsync(id);
for(const root of Object.values(refs))for(const t of root.findAllWithCriteria({types:["TEXT"]}))for(const s of t.getStyledTextSegments(["fontName"]))await figma.loadFontAsync(s.fontName);
const wallpaper=(await figma.getNodeByIdAsync("2:58")).fills;
const mobile=DEVICE.key==="phone";
function all(n){return [n,...("children"in n?n.findAll(()=>true):[])];}
function paint(n,k){n.fills=[figma.variables.setBoundVariableForPaint({type:"SOLID",color:{r:0,g:0,b:0}},"color",vm.get(k))];}
function sp(n,k,v){n[k]=v;if(vm.has("spacing/"+v))n.setBoundVariable(k,vm.get("spacing/"+v));}
function auto(p,name,dir,w,h,hug=false){const n=figma.createAutoLayout(dir);p.appendChild(n);n.name=name;n.fills=[];n.resize(w,h);n.layoutSizingHorizontal="FIXED";n.layoutSizingVertical=hug?"HUG":"FIXED";return n;}
async function text(p,label,style="Aegis/Body",tone="color/text",w){const n=figma.createText();p.appendChild(n);await n.setTextStyleIdAsync(sm.get(style).id);n.characters=label;n.name=label;n.textAutoResize="WIDTH_AND_HEIGHT";paint(n,tone);if(w){n.textAutoResize="HEIGHT";n.resize(w,24);n.layoutSizingHorizontal="FILL";n.textAutoResize="HEIGHT";n.layoutSizingVertical="HUG";}return n;}
function props(n,values){const out={};for(const[k,v]of Object.entries(values)){const prop=Object.keys(n.componentProperties).find(x=>x.split("#")[0]===k);if(prop)out[prop]=v;}n.setProperties(out);}
function ins(p,main,name,w,h){const n=main.createInstance();p.appendChild(n);n.name=name;if(w)n.resize(w,h);return n;}
async function nav(n,key){if(!key)return;await n.setReactionsAsync([{trigger:{type:"ON_CLICK"},actions:[{type:"NODE",destinationId:ids[key],navigation:"NAVIGATE",transition:{type:"DISSOLVE",easing:{type:"EASE_OUT"},duration:.15},resetScrollPosition:true}]}]);links.push({source:n.id,target:ids[key]});}
async function button(p,label,target,tone="Primary"){const n=ins(p,cc["button"+tone],label,p.width,48);n.layoutSizingHorizontal="FILL";props(n,{Label:label});await nav(n,target);return n;}
function avatar(p,initials){const n=ins(p,cc.avatar,"Avatar "+initials,64,64);props(n,{Initials:initials});return n;}
async function profile(p,name,detail,initials,target,selected=false){const n=ins(p,cc[selected?"profileSelected":"profileDefault"],"Profil · "+name,p.width,80);n.layoutSizingHorizontal="FILL";props(n,{Name:name,Detail:detail});props(n.children.find(x=>x.type==="INSTANCE"&&x.name==="Avatar"),{Initials:initials});await nav(n,target);return n;}
function field(p,label,value,state="Default"){const n=ins(p,cc["field"+state],label,p.width,88);n.layoutSizingHorizontal="FILL";props(n,{Label:label,Value:value});return n;}
async function info(p,iconId,title,body){const n=auto(p,title,"HORIZONTAL",p.width,48,true);n.layoutSizingHorizontal="FILL";sp(n,"itemSpacing",16);const ic=ins(n,refs[iconId],title+" Icon",24,24);const cp=auto(n,"Copy","VERTICAL",p.width-40,48,true);cp.layoutSizingHorizontal="FILL";await text(cp,title,"Aegis/Small strong","color/text",cp.width);if(body)await text(cp,body,"Aegis/Body","color/text-secondary",cp.width);return n;}
async function sidebar(p,def){const s=auto(p,"Sidebar","VERTICAL",320,720);paint(s,"color/sidebar");s.effects=em.get("Aegis/Glass sidebar").effects;for(const k of ["paddingLeft","paddingRight","paddingTop","paddingBottom"])sp(s,k,32);sp(s,"itemSpacing",32);
await text(s,"AegisOS","Aegis/Section");
const intro=auto(s,"Context","VERTICAL",256,100,true);sp(intro,"itemSpacing",16);await text(intro,def.group==="setup"?"Ein guter Start.":def.group==="login"?"Dein Bereich.":"Miteinander.\nUnabhängig.","Aegis/Heading","color/text",256);
await text(intro,def.group==="setup"?"In wenigen Schritten zu deinem persönlichen Konto.":def.group==="login"?"Ein Gerät. Eigene Konten.\nGetrennte Arbeitsbereiche.":"Jede Person hat ihre eigenen Apps, Dateien und Einstellungen.","Aegis/Body","color/text-secondary",256);
if(def.group==="setup"){const steps=auto(s,"Setup steps","VERTICAL",256,168,true);sp(steps,"itemSpacing",8);for(const [i,label]of ["Dein Profil","Passwort","Bereit"].entries()){const row=auto(steps,label,"HORIZONTAL",256,48);sp(row,"itemSpacing",16);row.counterAxisAlignItems="CENTER";sp(row,"paddingLeft",16);row.setBoundVariable("cornerRadius",vm.get("radius/panel"));if(def.step===i+1)paint(row,"color/subtle");await text(row,String(i+1).padStart(2,"0"),"Aegis/Body","color/text-secondary");await text(row,label,def.step===i+1?"Aegis/Small strong":"Aegis/Body");}}
const flex=auto(s,"Flexible space","VERTICAL",8,8);flex.layoutSizingVertical="FILL";
await info(s,"3:15","Lokal auf deinem Gerät","Kein Online-Konto nötig.");
return s;}
for(const {key,id} of FRAMES){
 const root=await figma.getNodeByIdAsync(id);if(root.children.length)throw new Error("Non-empty screen "+id);const def=DEFS[key];paint(root,"color/surface");if(mobile){root.cornerRadius=32;}else{const bg=figma.createRectangle();root.appendChild(bg);bg.name="Wallpaper · Silver Leaf";bg.resize(DEVICE.w,DEVICE.h);bg.fills=wallpaper;}
 const status=ins(root,refs[mobile?"60:1007":"3:143"],"Status bar",DEVICE.w,40);status.x=0;status.y=0;
 let body;
 if(mobile){
 const top=auto(root,"App header","HORIZONTAL",408,56);top.x=0;top.y=40;sp(top,"paddingLeft",24);sp(top,"paddingRight",24);top.counterAxisAlignItems="CENTER";await text(top,"AegisOS","Aegis/Small strong");const flex=auto(top,"Space","HORIZONTAL",8,8);flex.layoutSizingHorizontal="FILL";await text(top,def.group==="setup"?(def.step?def.step+" / 3":"Willkommen"):def.group==="login"?"Anmeldung":"Benutzer","Aegis/Body","color/text-secondary");
 body=auto(root,"Content","VERTICAL",360,688);body.x=24;body.y=120;sp(body,"itemSpacing",24);
 const gesture=ins(root,refs["70:1004"],"System gesture area",408,48);gesture.x=0;gesture.y=832;await gesture.setReactionsAsync([]);
 panels.push({key,id:root.id});
 }else{
 const card=auto(root,"System surface","HORIZONTAL",960,720);card.x=(DEVICE.w-960)/2;card.y=mobile?0:DEVICE.key==="tablet"?64:136;card.cornerRadius=24;card.setBoundVariable("cornerRadius",vm.get("radius/floating"));card.effects=em.get("Aegis/Window").effects;card.clipsContent=true;
 await sidebar(card,def);
 const content=auto(card,"Main","VERTICAL",640,720);paint(content,"color/surface");for(const k of ["paddingLeft","paddingRight","paddingTop","paddingBottom"])sp(content,k,48);
 body=auto(content,"Content","VERTICAL",544,624);sp(body,"itemSpacing",24);panels.push({key,id:card.id});
 }
 const head=auto(body,"Heading","VERTICAL",body.width,88,true);head.layoutSizingHorizontal="FILL";sp(head,"itemSpacing",8);await text(head,def.title,"Aegis/Heading","color/text",head.width);await text(head,def.sub,"Aegis/Body","color/text-secondary",head.width);
 const fields=auto(body,"Form and details","VERTICAL",body.width,8,true);fields.layoutSizingHorizontal="FILL";sp(fields,"itemSpacing",16);
 if(def.kind==="welcome"){
  const tile=auto(fields,"Aegis tile","HORIZONTAL",80,80);paint(tile,"color/subtle");tile.setBoundVariable("cornerRadius",vm.get("radius/tile"));tile.primaryAxisAlignItems="CENTER";tile.counterAxisAlignItems="CENTER";ins(tile,refs["3:15"],"Shield",40,40);
  await info(fields,"3:6","Ein System für deine Geräte","Dieselbe Ruhe. Überall.");
  await info(fields,"3:18","Deine Daten bleiben bei dir","Ein lokales Konto ist alles, was du brauchst.");
 }else if(def.kind==="profile"){
  avatar(fields,"SG");field(fields,"Dein Name","Simeon");await info(fields,"3:15","Das erste Konto ist Administrator","Du kannst später weitere Benutzer hinzufügen.");
 }else if(def.kind==="password"){
  field(fields,"Passwort","••••••••••••");field(fields,"Passwort wiederholen","••••••••••••");await text(fields,"Verwende ein Passwort, das du dir gut merken kannst.","Aegis/Body","color/text-secondary",fields.width);
 }else if(def.kind==="ready"){
  await profile(fields,"Simeon","Administrator · Konto eingerichtet","SG",null,true);await info(fields,"3:15","Einrichtung abgeschlossen","Dein Konto ist angelegt. Die Anmeldung erfolgt im nächsten Schritt.");
 }else if(["login","login_maya","locked","error","throttled"].includes(def.kind)){
  avatar(fields,def.kind==="login_maya"?"MA":"SG");
  field(fields,"Passwort",def.kind==="throttled"?"Vorübergehend gesperrt":"••••••••••••",def.kind==="error"?"Error":def.kind==="throttled"?"Disabled":"Default");
  if(def.kind==="error")await text(fields,"Das Passwort stimmt nicht.\nBitte versuche es erneut.","Aegis/Body","color/text",fields.width);
  if(def.kind==="throttled")await text(fields,"Zu viele Versuche. Du kannst dich in 30 Sekunden erneut anmelden.","Aegis/Body","color/text",fields.width);
  if(def.kind==="locked")await text(fields,"Deine Apps und Dateien bleiben geöffnet.","Aegis/Body","color/text-secondary",fields.width);
 }else if(def.kind==="admin"){
  await profile(fields,"Simeon","Administrator","SG",null,true);field(fields,"Dein Administrator-Passwort","••••••••••••");await text(fields,"Die Bestätigung gilt nur für das Anlegen dieses Kontos.","Aegis/Body","color/text-secondary",fields.width);
 }else if(def.kind==="new_user"){
  field(fields,"Name","Maya");field(fields,"Passwort","••••••••••••");field(fields,"Passwort wiederholen","••••••••••••");await text(fields,"Standardkonto · ohne Administratorrechte","Aegis/Body","color/text-secondary",fields.width);
 }else if(def.kind==="added"){
  await profile(fields,"Maya","Standardkonto · Bereit zur Anmeldung","MA",null,true);await text(fields,"Du bleibst als Simeon angemeldet.","Aegis/Body","color/text-secondary",fields.width);
 }else if(def.kind==="users"){
  await profile(fields,"Simeon","Administrator · Sitzung vorhanden","SG","login_simeon");await profile(fields,"Maya","Standardkonto","MA","login_maya");
  await text(fields,"Zum Öffnen eines Bereichs ist das jeweilige Passwort erforderlich.","Aegis/Body","color/text-secondary",fields.width);
 }else if(["single","accounts","maya"].includes(def.kind)){
  await profile(fields,def.kind==="maya"?"Maya":"Simeon",def.kind==="maya"?"Standardkonto · Angemeldet":"Administrator · Angemeldet",def.kind==="maya"?"MA":"SG",null,true);
  if(def.kind==="accounts")await profile(fields,"Maya","Standardkonto","MA","users");
  await info(fields,"3:15","Eigener Arbeitsbereich",def.kind==="maya"?"Deine Apps, Dateien und Einstellungen gehören zu deinem Konto.":"Andere Benutzer melden sich mit ihrem eigenen Passwort an.");
  if(def.kind!=="maya")await button(fields,"Sitzung sperren","locked","Secondary");
  if(def.kind==="accounts")await text(fields,"Beim Benutzerwechsel bleibt deine Sitzung im Hintergrund erhalten.","Aegis/Body","color/text-secondary",fields.width);
 }
 const flex=auto(body,"Flexible space","VERTICAL",8,8);flex.layoutSizingVertical="FILL";
 const footer=auto(body,"Actions","VERTICAL",body.width,48,true);footer.layoutSizingHorizontal="FILL";sp(footer,"itemSpacing",8);
 if(def.primary)await button(footer,def.primary[0],def.primary[1],def.kind==="throttled"?"Disabled":"Primary");
 if(def.secondary)await button(footer,def.secondary[0],def.secondary[1],"Quiet");
 if(def.kind==="users")await text(footer,"AegisOS · Lokale Anmeldung","Aegis/Body","color/text-secondary",footer.width);
 root.placeholder=false;mutated.add(root.id);for(const n of root.findAll(()=>true))created.add(n.id);
}
figma.currentPage.flowStartingPoints=[{nodeId:ids.welcome,name:"Erster Start"},{nodeId:ids.users,name:"Login · Multiuser"},{nodeId:ids.account_single,name:"Benutzer hinzufügen"},{nodeId:ids.error,name:"Passwortfehler"},{nodeId:ids.throttled,name:"Anmeldesperre"}];
return {createdNodeIds:[...created],mutatedNodeIds:[...mutated],device:DEVICE.key,panels,links,screens:FRAMES.map(f=>({key:f.key,id:f.id})),counts:{screens:FRAMES.length,links:links.length}};

