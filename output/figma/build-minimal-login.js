
await figma.setCurrentPageAsync(await figma.getNodeByIdAsync(D.page));
const vm=new Map((await figma.variables.getLocalVariablesAsync()).map(v=>[v.name,v]));
const sm=new Map((await figma.getLocalTextStylesAsync()).map(s=>[s.name,s]));
for(const s of sm.values())await figma.loadFontAsync(s.fontName);
const roots=[];for(const f of BEFORE){const r=await figma.getNodeByIdAsync(f.id);roots.push(r);for(const n of r.findAllWithCriteria({types:["TEXT"]}))for(const s of n.getStyledTextSegments(["fontName"]))await figma.loadFontAsync(s.fontName);}
const comps={};for(const [k,id]of Object.entries(COMPS))comps[k]=await figma.getNodeByIdAsync(id);
const quiet=await figma.getNodeByIdAsync("137:1006");const wallpaper=(await figma.getNodeByIdAsync("2:58")).fills;
const created=[],changed=[],removed=[],reports=[],links=[];
function color(k){return [figma.variables.setBoundVariableForPaint({type:"SOLID",color:{r:0,g:0,b:0}},"color",vm.get(k))];}
function sp(n,k,v){n[k]=v;if(vm.has("spacing/"+v))n.setBoundVariable(k,vm.get("spacing/"+v));}
function auto(p,name,dir,w,h,hug=false){const n=figma.createAutoLayout(dir);p.appendChild(n);n.name=name;n.fills=[];n.resize(w,h);n.layoutSizingHorizontal="FIXED";n.layoutSizingVertical=hug?"HUG":"FIXED";return n;}
function props(n,values){const p={};for(const [k,v]of Object.entries(values)){const found=Object.keys(n.componentProperties).find(x=>x.split("#")[0]===k);if(found)p[found]=v;}n.setProperties(p);}
async function text(p,label,style,w){const n=figma.createText();p.appendChild(n);await n.setTextStyleIdAsync(sm.get(style).id);n.characters=label;n.name=label||"State message";n.fills=color("color/text");n.resize(w,24);n.textAutoResize="HEIGHT";n.layoutSizingHorizontal="FILL";n.layoutSizingVertical="HUG";n.textAlignHorizontal="CENTER";return n;}
async function nav(n,dest){await n.setReactionsAsync([{trigger:{type:"ON_CLICK"},actions:[{type:"NODE",destinationId:IDS[dest],navigation:"NAVIGATE",transition:{type:"DISSOLVE",easing:{type:"EASE_OUT"},duration:.15},resetScrollPosition:true}]}]);links.push({source:n.id,target:IDS[dest]});}
function av(p,name){const a=comps.avatar.createInstance();p.appendChild(a);a.name="Profile · "+name;props(a,{Initials:name==="Maya"?"MA":"SG"});return a;}
for(const old of BEFORE){
 const root=await figma.getNodeByIdAsync(old.id);
 for(const item of old.children.filter(x=>x.name==="System surface"||x.name==="Content"||x.name==="App header")){
 const n=await figma.getNodeByIdAsync(item.id);removed.push(n.id,...n.findAll(()=>true).map(c=>c.id));n.remove();
 }
 let bg=root.children.find(n=>n.name==="Wallpaper · Silver Leaf");
 if(!bg){bg=figma.createRectangle();root.insertChild(0,bg);bg.name="Wallpaper · Silver Leaf";bg.resize(D.w,D.h);bg.fills=wallpaper;created.push(bg.id);}
 bg.resize(D.w+128,D.h+128);bg.x=-64;bg.y=-64;bg.effects=[{type:"LAYER_BLUR",radius:24,visible:true}];changed.push(bg.id);
 const veil=figma.createRectangle();root.insertChild(1,veil);veil.name="Wallpaper · Soft contrast";veil.resize(D.w,D.h);veil.fills=color("color/surface");veil.opacity=.24;created.push(veil.id);
 const widget=auto(root,old.key==="users"?"User picker":"Login","VERTICAL",320,304);widget.counterAxisAlignItems="CENTER";sp(widget,"itemSpacing",24);widget.x=(D.w-320)/2;widget.y=Math.round(((D.h-304)/2-32)/8)*8;
 if(old.key==="users"){
 widget.resize(320,136);widget.y=Math.round(((D.h-136)/2-32)/8)*8;
 const choices=auto(widget,"Profiles","HORIZONTAL",320,136);choices.primaryAxisAlignItems="CENTER";sp(choices,"itemSpacing",32);
 for(const name of ["Simeon","Maya"]){const choice=auto(choices,"Choose "+name,"VERTICAL",128,136);choice.counterAxisAlignItems="CENTER";sp(choice,"paddingTop",8);sp(choice,"paddingBottom",8);sp(choice,"itemSpacing",16);av(choice,name);await text(choice,name,"Aegis/Small strong",128);await nav(choice,name==="Maya"?"login_maya":"login_simeon");}
 }else{
 const profile=auto(widget,"Profile","VERTICAL",320,128);profile.counterAxisAlignItems="CENTER";sp(profile,"itemSpacing",16);const name=old.key==="login_maya"?"Maya":"Simeon";av(profile,name);await text(profile,name,"Aegis/Section",320);
 const form=auto(widget,"Credential","VERTICAL",304,80);sp(form,"itemSpacing",8);
 const state=old.key==="error"?"Error":old.key==="throttled"?"Disabled":"Default";
 const entry=comps[state].createInstance();form.appendChild(entry);entry.name="Password";const submit=entry.children.find(n=>n.name==="Submit");submit.name=old.key==="locked"?"Entsperren":"Anmelden";if(state!=="Disabled")await nav(submit,old.key==="first_login"?"account_single":old.key==="login_maya"?"maya_session":"accounts");
 const msg=auto(form,"Feedback","VERTICAL",304,24);
 const message=old.key==="error"?"Passwort stimmt nicht.":old.key==="throttled"?"Erneut in 30 Sekunden.":old.key==="locked"?"Gesperrt":"";
 if(message)await text(msg,message,"Aegis/Body",304);
 if(old.key!=="first_login"){const switcher=quiet.createInstance();widget.appendChild(switcher);switcher.name="Benutzer wechseln";switcher.resize(208,48);props(switcher,{Label:"Benutzer wechseln"});await nav(switcher,"users");}
 }
 root.placeholder=false;changed.push(root.id);created.push(widget.id,...widget.findAll(()=>true).map(n=>n.id));
 reports.push({key:old.key,id:root.id,widget:widget.id,x:widget.x,y:widget.y,w:widget.width,h:widget.height,texts:widget.findAllWithCriteria({types:["TEXT"]}).filter(n=>n.visible).map(n=>n.characters)});
}
return {createdNodeIds:created,mutatedNodeIds:changed,removedNodeIds:removed,reports,links};

