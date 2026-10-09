/**
 * bindVariablesToComponent
 *
 * Binds design token variables to the visual properties of a component node.
 * Supports fills, strokes, all padding directions, item spacing, and corner radius.
 * Only binds properties for which a variable ID is provided in `bindings`.
 *
 * This function should be called on each variant individually within a component
 * set, OR on the component set itself for properties shared by all variants.
 *
 * @param {ComponentNode | FrameNode | RectangleNode} component
 *   The Figma node to mutate. Usually a ComponentNode or one of its children.
 * @param {{
 *   fills?: string,
 *   strokes?: string,
 *   paddingTop?: string,
 *   paddingBottom?: string,
 *   paddingLeft?: string,
 *   paddingRight?: string,
 *   itemSpacing?: string,
 *   cornerRadius?: string
 * }} bindings
 *   Each key is a visual property name; each value is a Figma Variable ID
 *   (e.g. "VariableID:123:456"). Omit a key to skip binding that property.
 * @returns {Promise<{ mutatedNodeIds: string[] }>}
 *   List of node IDs that were mutated (for audit/validation purposes).
 */
async function bindVariablesToComponent(component, bindings) {
  const mutatedNodeIds = []

  if (!component) {
    return { mutatedNodeIds }
  }

  // Batch every getVariableByIdAsync call upfront in a single Promise.all rather
  // than awaiting per-property — the lookups are independent and IPC-bound.
  const floatBindings = [
    ['paddingTop', 'paddingTop'],
    ['paddingBottom', 'paddingBottom'],
    ['paddingLeft', 'paddingLeft'],
    ['paddingRight', 'paddingRight'],
    ['itemSpacing', 'itemSpacing'],
    ['cornerRadius', 'cornerRadius'],
  ]

  const requestedIds = []
  if (bindings.fills) requestedIds.push(['fills', bindings.fills])
  if (bindings.strokes) requestedIds.push(['strokes', bindings.strokes])
  for (const [bindingKey] of floatBindings) {
    if (bindings[bindingKey]) requestedIds.push([bindingKey, bindings[bindingKey]])
  }

  const resolved = await Promise.all(
    requestedIds.map(([, id]) => figma.variables.getVariableByIdAsync(id)),
  )
  const varByKey = {}
  for (let i = 0; i < requestedIds.length; i++) {
    varByKey[requestedIds[i][0]] = resolved[i]
  }

  const markMutated = () => {
    if (!mutatedNodeIds.includes(component.id)) {
      mutatedNodeIds.push(component.id)
    }
  }

  // --- Fills ---
  const fillVar = varByKey.fills
  if (fillVar) {
    const existingFills = component.fills
    if (Array.isArray(existingFills) && existingFills.length > 0) {
      // Bind the color of the first fill to the variable
      const boundFill = figma.variables.setBoundVariableForPaint(existingFills[0], 'color', fillVar)
      component.fills = [boundFill, ...existingFills.slice(1)]
    } else {
      // No existing fill — create a solid fill bound to the variable
      const boundFill = figma.variables.setBoundVariableForPaint(
        { type: 'SOLID', color: { r: 0.5, g: 0.5, b: 0.5 } },
        'color',
        fillVar,
      )
      component.fills = [boundFill]
    }
    markMutated()
  }

  // --- Strokes ---
  const strokeVar = varByKey.strokes
  if (strokeVar) {
    const existingStrokes = component.strokes
    if (Array.isArray(existingStrokes) && existingStrokes.length > 0) {
      const boundStroke = figma.variables.setBoundVariableForPaint(
        existingStrokes[0],
        'color',
        strokeVar,
      )
      component.strokes = [boundStroke, ...existingStrokes.slice(1)]
    } else {
      const boundStroke = figma.variables.setBoundVariableForPaint(
        { type: 'SOLID', color: { r: 0.5, g: 0.5, b: 0.5 } },
        'color',
        strokeVar,
      )
      component.strokes = [boundStroke]
    }
    markMutated()
  }

  // --- Spacing properties (FLOAT variables bound via setBoundVariable) ---
  for (const [bindingKey, figmaProp] of floatBindings) {
    const variable = varByKey[bindingKey]
    if (variable) {
      component.setBoundVariable(figmaProp, variable)
      markMutated()
    }
  }

  return { mutatedNodeIds }
}


await figma.setCurrentPageAsync(await figma.getNodeByIdAsync("2:2"));
const styles=await figma.getLocalTextStylesAsync(),vars=await figma.variables.getLocalVariablesAsync();
const sm=new Map(styles.map(s=>[s.name,s])),vm=new Map(vars.map(v=>[v.name,v]));
for(const s of styles)await figma.loadFontAsync(s.fontName);
const roots=[],created=[];
function color(name){return [figma.variables.setBoundVariableForPaint({type:"SOLID",color:{r:0,g:0,b:0}},"color",vm.get(name))];}
function sp(n,key,v){n[key]=v;if(vm.has("spacing/"+v))n.setBoundVariable(key,vm.get("spacing/"+v));}
async function txt(p,name,text,style="Aegis/Body",tone="color/text"){const n=figma.createText();p.appendChild(n);await n.setTextStyleIdAsync(sm.get(style).id);n.characters=text;n.name=name;n.fills=color(tone);n.textAutoResize="WIDTH_AND_HEIGHT";return n;}
function comp(name,w,h,dir="HORIZONTAL"){const n=figma.createComponent();n.name=name;n.layoutMode=dir;n.resize(w,h);n.fills=[];n.primaryAxisSizingMode="FIXED";n.counterAxisSizingMode="FIXED";roots.push(n);return n;}
function frame(p,name,w,h,dir="HORIZONTAL"){const n=figma.createAutoLayout(dir);p.appendChild(n);n.name=name;n.resize(w,h);n.fills=[];return n;}
function prop(c,n,label,val){const k=c.addComponentProperty(label,"TEXT",val);n.componentPropertyReferences={characters:k};return k;}
const map={};const sets=[];
for(const tone of ["Primary","Secondary","Quiet","Disabled"]){
 const c=comp("Tone="+tone,360,48);c.primaryAxisAlignItems="CENTER";c.counterAxisAlignItems="CENTER";
 await bindVariablesToComponent(c,{cornerRadius:vm.get("radius/search").id,paddingLeft:vm.get("spacing/24").id,paddingRight:vm.get("spacing/24").id});
 c.fills=tone==="Quiet"?[]:color(tone==="Primary"?"color/text":"color/subtle");
 const t=await txt(c,"Label","Weiter","Aegis/Small strong",tone==="Primary"?"color/on-selected":tone==="Disabled"?"color/text-secondary":"color/text");prop(c,t,"Label","Weiter");map["button"+tone]=c.id;
}
const bs=figma.combineAsVariants(roots.splice(0),figma.currentPage);bs.name="Auth / Button";bs.x=7040;bs.y=200;bs.children.forEach((n,i)=>{n.x=24;n.y=24+i*64;});bs.resize(408,288);sets.push(bs);
for(const state of ["Default","Error","Disabled"]){
 const c=comp("State="+state,360,88,"VERTICAL");sp(c,"itemSpacing",8);
 const l=await txt(c,"Label","Passwort","Aegis/Small strong");prop(c,l,"Label","Passwort");
 const field=frame(c,"Input surface",360,56);field.layoutSizingHorizontal="FILL";field.counterAxisAlignItems="CENTER";sp(field,"paddingLeft",16);sp(field,"paddingRight",16);field.fills=color("color/subtle");field.setBoundVariable("cornerRadius",vm.get("radius/field"));
 if(state==="Error"){field.strokes=color("color/text");field.strokeWeight=2;field.strokeAlign="INSIDE";}
 const v=await txt(field,"Value","••••••••••••","Aegis/Body",state==="Disabled"?"color/text-secondary":"color/text");prop(c,v,"Value","••••••••••••");v.textAutoResize="HEIGHT";v.resize(328,24);v.layoutSizingHorizontal="FILL";map["field"+state]=c.id;
}
const fs=figma.combineAsVariants(roots.splice(0),figma.currentPage);fs.name="Auth / Field";fs.x=7512;fs.y=200;fs.children.forEach((n,i)=>{n.x=24;n.y=24+i*112;});fs.resize(408,384);sets.push(fs);
const av=comp("Auth / Avatar",64,64);av.primaryAxisAlignItems="CENTER";av.counterAxisAlignItems="CENTER";av.fills=color("color/subtle");av.setBoundVariable("cornerRadius",vm.get("radius/tile"));const initial=await txt(av,"Initials","SG","Aegis/Section");prop(av,initial,"Initials","SG");av.x=7984;av.y=200;map.avatar=av.id;roots.splice(0);
for(const state of ["Default","Selected","Disabled"]){
 const c=comp("State="+state,360,80);c.counterAxisAlignItems="CENTER";sp(c,"itemSpacing",16);sp(c,"paddingLeft",8);sp(c,"paddingRight",8);c.fills=state==="Selected"?color("color/subtle"):[];c.setBoundVariable("cornerRadius",vm.get("radius/panel"));
 const a=av.createInstance();c.appendChild(a);a.name="Avatar";const cp=frame(c,"Copy",240,48,"VERTICAL");cp.layoutSizingHorizontal="FILL";
 const name=await txt(cp,"Name","Simeon","Aegis/Small strong");prop(c,name,"Name","Simeon");name.textAutoResize="HEIGHT";name.resize(240,24);name.layoutSizingHorizontal="FILL";
 const detail=await txt(cp,"Detail","Administrator","Aegis/Body","color/text-secondary");prop(c,detail,"Detail","Administrator");detail.textAutoResize="HEIGHT";detail.resize(240,24);detail.layoutSizingHorizontal="FILL";
 if(state==="Disabled")c.opacity=.55;
 const chevron=(await figma.getNodeByIdAsync("3:24")).createInstance();c.appendChild(chevron);chevron.resize(24,24);map["profile"+state]=c.id;
}
const ps=figma.combineAsVariants(roots.splice(0),figma.currentPage);ps.name="Auth / Profile";ps.x=8160;ps.y=200;ps.children.forEach((n,i)=>{n.x=24;n.y=24+i*104;});ps.resize(408,360);sets.push(ps);
for(const n of [...sets,av]){n.description="Welcome und lokale Anmeldung. Manrope, Aegis-Farbvariablen und Abstände im 8-px-Raster; native editierbare Komponenten.";created.push(n.id,...n.findAll(()=>true).map(c=>c.id));}
return {createdNodeIds:created,mutatedNodeIds:[],map,sets:sets.map(n=>({id:n.id,name:n.name,variants:n.children.length})),avatar:{id:av.id,name:av.name}};

