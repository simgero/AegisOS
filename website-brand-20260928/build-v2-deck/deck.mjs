import fs from 'node:fs/promises';
import { Presentation, PresentationFile } from '@oai/artifact-tool';
import { resolvePresentationFont } from '/Users/simeongerodetti/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations/container_tools/artifact_tool_utils.mjs';

const root = '/Users/simeongerodetti/AegisOS/website-brand-20260928';
const build = root + '/build-v2-deck';
const C = { paper:'#F6F7F9', ink:'#161B22', lime:'#C4F15A', muted:'#58616A', pale:'#B9C1C9' };
const font = resolvePresentationFont({fontFamily:'Rubik'});
console.log('Font:',font);
const p = Presentation.create({slideSize:{width:1280,height:720}});
const wallpaper = await fs.readFile(root+'/output-v2/brand-assets/wallpaper.png');
const symbol = await fs.readFile(root+'/output-v2/brand-assets/aegis-symbol.png');
const aospScreen = await fs.readFile('/Users/simeongerodetti/AegisOS/out/qemu-first-boot/mouse-1/mouse-after.png');
const repo = 'https://github.com/simgero/AegisOS/tree/codex/aosp-cloud-builder';
const sources = {
  brief: 'docs/architecture/phase-1-developer-brief.md',
  progress: 'docs/phase-1-progress.md',
  runtime: 'runtime/README.md',
  passwords: 'docs/identity-platform-test.md',
  boot: 'docs/qemu-first-boot.md'
};

function txt(s,t,x,y,w,h,size=28,color=C.ink,bold=false,opts={}) {
  const a=s.shapes.add({geometry:'textbox',position:{left:x,top:y,width:w,height:h},fill:'none',line:{fill:'none',width:0}});
  a.text=t;
  a.text.style={typeface:font,fontSize:size,color,bold,autoFit:'none',wrap:'none',insets:{left:0,right:0,top:0,bottom:0},...opts};
  return a;
}
function base(n,{dark=false,bg,tag='',footer=true}={}) {
  const s=p.slides.add();
  s.background.fill=bg ?? (dark?C.ink:C.paper);
  const fg=dark?C.paper:C.ink;
  txt(s,'AEGIS OS',64,39,280,32,21,fg,true);
  if(tag) txt(s,tag,700,41,516,28,17,dark?C.pale:C.muted,false,{alignment:'right'});
  if(footer) txt(s,String(n).padStart(2,'0'),1146,661,70,26,17,dark?C.pale:C.muted,false,{alignment:'right'});
  return s;
}
function notes(s,content,refs){
  s.speakerNotes.textFrame.setText(content+'\n\nQuellen im Projektstand vom 28. September 2026:\n'+refs.map(r=>'/Users/simeongerodetti/AegisOS/'+r).join('\n'));
}

// 1. Cover: typography remains native; the wallpaper is original brand artwork.
let s=p.slides.add(); s.background.fill=C.paper;
s.images.add({blob:wallpaper,contentType:'image/png',fit:'cover',position:{left:760,top:0,width:520,height:720}});
s.images.add({blob:symbol,contentType:'image/png',fit:'contain',position:{left:54,top:28,width:62,height:62}});
txt(s,'AEGIS OS',128,45,400,32,25,C.ink,true);
txt(s,'Android-Apps.\nGNU/Linux-\nWerkzeuge.',64,150,720,290,80,C.ink,true,{lineSpacing:0.98});
txt(s,'Ein Betriebssystem für\nbeide Softwarewelten.',68,467,660,98,35,C.ink,false,{lineSpacing:1.06});
txt(s,'In Entwicklung',68,615,640,27,19,C.ink,true);
txt(s,'Projektstand 28. September 2026',68,648,640,26,18,C.muted);
notes(s,'Aegis OS ist ein Entwicklungsprojekt. Der Leitgedanke beschreibt das Ziel, Android-Anwendungen und einen GNU/Linux-Userspace in einem AOSP-basierten Betriebssystem zu verbinden. Die GNU/Linux-Runtime ist noch nicht ausführbar. Die Abbildung ist ein originales, KI-generiertes Markenmotiv und zeigt keine implementierte Oberfläche.',[sources.brief,sources.progress,sources.runtime]);

// 2. Concrete intended use, with no untested app compatibility claim.
s=base(2,{tag:'Geplanter Einsatz'});
txt(s,'Android im Alltag\nGNU/Linux bei der Arbeit',64,126,1152,144,56,C.ink,true,{lineSpacing:1.02});
txt(s,'Android-Apps',68,341,510,54,38,C.ink,true);
txt(s,'Kommunikation, Web und Medien\nmit vertrauten mobilen Anwendungen.',68,419,515,112,28,C.muted,false,{lineSpacing:1.15});
txt(s,'+',598,369,72,88,69,C.muted,false,{alignment:'center'});
txt(s,'Bash, apt und glibc',716,341,500,54,38,C.ink,true);
txt(s,'Skripte ausführen und GNU-Werkzeuge\naus einer Debian-Basis nutzen.',716,419,500,112,28,C.muted,false,{lineSpacing:1.15});
txt(s,'Ziel: beide Softwarewelten auf demselben Gerät nutzen.',68,576,1135,42,30,C.ink);
txt(s,'Diese Beispiele sind geplant. Konkrete App-Kompatibilität ist noch nicht nachgewiesen.',68,638,1080,34,20,C.muted);
notes(s,'Die genannten Abläufe sind Zielbilder für technische frühe Anwender. Es gibt keine getestete Kompatibilitätsliste für Android-Apps, Google Play, Banking-Apps oder grafische Linux-Anwendungen. Debian 13.7 ARM64 ist als unverändertes Archiv importiert und geprüft. Bash, apt und glibc sind darin vorhanden, aber noch nicht im AOSP-Gast ausgeführt.',[sources.brief,sources.runtime,sources.progress]);

// 3. The planned architecture as a typographic explanation, not an implemented UI.
s=base(3,{dark:true,tag:'Geplante Architektur'});
txt(s,'Ein Betriebssystem\nEin gemeinsamer Kernel',64,118,1152,151,60,C.paper,true,{lineSpacing:1.0});
txt(s,'Android',68,317,493,57,43,C.paper,true);
txt(s,'Apps und Android-Framework',68,389,500,44,27,C.pale);
txt(s,'GNU/Linux',703,317,513,57,43,C.paper,true);
txt(s,'Debian-Userspace in isolierten Kontexten',703,389,513,77,26,C.pale);
txt(s,'AOSP verwaltet Anmeldung, Rechte und persönlichen Speicher.',68,487,1134,49,30,C.paper);
txt(s,'Linux-Kernel',68,554,820,73,57,C.lime,true);
txt(s,'Die GNU-Runtime nutzt den AOSP-Kernel. Eine zusätzliche Linux-VM ist nicht vorgesehen.',68,646,1080,38,20,C.pale);
notes(s,'AOSP ist die einzige Instanz für persönliche Identität, Authentifizierung, Berechtigungen und CE-Schlüsselverwaltung. GNU/Linux führt keine zweite persönliche Anmeldung ein. Die Runtime soll den Kernel des gestarteten AOSP verwenden. QEMU ist die Entwicklungsumgebung für das gesamte OS und keine zusätzliche Linux-VM innerhalb von AOSP. GNU-Prozesse benötigen eine korrekte UID/GID-Zuordnung, Namespaces, SELinux und eine an AOSP gebundene Lebenszyklusverwaltung. Diese Integration steht noch aus.',[sources.brief,sources.progress,sources.runtime]);

// 4. Package scope as a useful work scenario, rather than multiuser marketing.
s=base(4,{tag:'Geplante Paketverwaltung'});
txt(s,'Die passende Version\nfür die eigene Arbeit',64,122,1152,145,57,C.ink,true,{lineSpacing:1.0});
txt(s,'Für mich',68,327,510,56,42,C.ink,true);
txt(s,'--scope user',68,391,500,40,25,C.muted);
txt(s,'Eine private Paketversion mit passenden\nAbhängigkeiten gilt im eigenen Kontext.',68,452,526,94,27,C.ink,false,{lineSpacing:1.13});
txt(s,'Für alle',707,327,509,56,42,C.ink,true);
txt(s,'--scope all',707,391,509,40,25,C.muted);
txt(s,'Ein gemeinsamer Paketbestand steht\nallen persönlichen Kontexten bereit.',707,452,509,94,27,C.ink,false,{lineSpacing:1.13});
txt(s,'Gemeinsame Updates sollen private Versionen erhalten.',68,573,1118,44,29,C.ink,true);
txt(s,'Jede Paketaktion braucht AOSP-Adminautorisierung. Die Umsetzung steht noch aus.',68,638,1092,38,20,C.muted);
notes(s,'Die Spezifikation verlangt persönliche und gemeinsame Paketinstallationen, inklusive explizit ausgewählter Versionen und passender Abhängigkeiten. Eine private Version soll im eigenen Kontext wirksam bleiben, auch wenn sich der gemeinsame Bestand ändert. Gemeinsame Updates müssen kontrolliert aktivieren, laufende Kontexte konsistent halten und Konflikte sichtbar melden. Sowohl user als auch all benötigen eine frische, aktionsgebundene AOSP-Adminautorisierung. Paketverwaltung und Konsistenz sind noch nicht implementiert oder im Gast nachgewiesen. Die Scope-Angaben zeigen die geplante CLI-Syntax und sind keine Produktoberfläche.',[sources.brief,sources.progress,sources.runtime]);

// 5. Native editable status table with exact boundaries.
s=base(5,{tag:'Stand 28. September 2026'});
txt(s,'Was heute belegt ist',64,124,1152,85,61,C.ink,true);
const table=s.tables.add({rows:3,columns:2,left:64,top:264,width:861,height:345,columnWidths:[203,658],values:[
  ['Nachgewiesen','Android bootet mit sichtbarer Oberfläche.\nAOSP-Passwort-, CE-Sperr- und Löschtests\nmit einem Testbenutzer.'],
  ['Vorbereitet','Debian-13.7-Archiv: 78 Pakete geprüft.\nIdentitäts- und Runtime-Code im Quelltext,\nnoch nicht im Gast integriert.'],
  ['Noch offen','GNU-Ausführung und Systemintegration.\nPakete sowie der vollständige Nachweis\nmit zwei Benutzern.']
]});
table.styleOptions={headerRow:false,bandedRows:false,firstColumn:false};
table.borders.assign({style:'solid',fill:C.paper,width:0});
for(let r=0;r<3;r++) {
  table.rows[r].height=115;
  for(let c=0;c<2;c++) {
    const cell=table.getCell(r,c);
    cell.fill=c===0?C.ink:C.paper;
    cell.text.style={typeface:font,fontSize:c===0?22:22.5,bold:c===0,color:c===0?(r===0?C.lime:C.paper):C.ink,insets:{top:15,bottom:15,left:c===0?18:22,right:8},verticalAlignment:'middle',autoFit:'none',wrap:'square'};
  }
}
txt(s,'GNU-Runtime noch nicht ausführbar. Neuer Code unkompiliert.',68,640,850,38,20,C.muted);
s.images.add({blob:aospScreen,contentType:'image/png',fit:'contain',position:{left:967,top:234,width:217,height:386}});
txt(s,'Realer AOSP-Gast in QEMU',954,633,262,27,17,C.muted,false,{alignment:'center'});
notes(s,'Primäre Statusquelle ist docs/phase-1-progress.md, Stand 28. September 2026. Nachgewiesen: lokaler Android-Boot mit sichtbarer Oberfläche und ADB. Ein persönlicher AOSP-Testbenutzer: falsches Passwort abgewiesen, CE-Sperre nach Benutzerstopp, Datenzugriff mit richtigem Passwort, Passwortwechsel sowie anschließende Plattformlöschung geprüft. Dies erfolgte über AOSP-Dialoge und Plattformbefehle, nicht über AEGIS. Vorbereitet: Identitäts-/CLI-Integration und Runtime-Bausteine im Quelltext, unkompiliert, nicht im Gast. Debian-Basis ist als Archiv geprüft, weder extrahiert noch ausgeführt. Paketverwaltung, Runtime-Integration und kompletter Zwei-Benutzer-Lebenszyklus sind offen. CE bezeichnet AOSPs an die Benutzeranmeldung gebundenen verschlüsselten Speicher. Das Bild ist eine unveränderte echte Bildschirmaufnahme der AOSP-Einstellungen im QEMU-Gast aus dem Lauf mouse-1. Es zeigt keine eigene Aegis-Oberfläche.',[sources.progress,sources.runtime,sources.passwords,sources.boot,'docs/local-adb.md','out/qemu-first-boot/mouse-1/mouse-after.png']);

// 6. One near-term proof, with prerequisites and observable acceptance.
s=base(6,{bg:C.lime,tag:'Nächster Integrationsnachweis'});
txt(s,'Nächster Nachweis:\nGNU/Linux im AOSP-Gast',64,117,1152,149,60,C.ink,true,{lineSpacing:1.0});
const steps=[
  ['01','Kernel integrieren','User-, PID-, Mount- und IPC-Isolation im Gast prüfen.'],
  ['02','Runtime anbinden','AOSP-Identität, persönliche Mounts und SELinux zusammenführen.'],
  ['03','Den Ablauf belegen','Bash ausführen, Daten nach Neustart erhalten und Logout bestätigen.']
];
steps.forEach((r,i)=>{const y=324+i*99;txt(s,r[0],68,y,70,45,25,C.ink,true);txt(s,r[1],180,y,995,44,31,C.ink,true);txt(s,r[2],180,y+43,1005,39,24,C.ink);});
txt(s,'Anschließend: Paketbereiche und Isolation mit zwei AOSP-Benutzern praktisch nachweisen.',68,650,1100,35,20,C.ink);
notes(s,'Diese Folie beschreibt die nächsten notwendigen Nachweise, nicht bereits verfügbare Funktionen. Der laufende Kernel hat laut Statusbericht keine User-/PID-Namespaces und kein System-V-IPC. Ein passender Kernelbuild einschließlich Modulen und anschließende Gasttests sind erforderlich. Broker, Mounts, SELinux und die vollständige AOSP-Logout-/CE-Koordination fehlen. Der Lauf muss an einen echten authentifizierten AOSP-Benutzer gebunden sein, Bash tatsächlich ausführen und persistenten persönlichen Speicher korrekt behandeln. Prozessende allein bestätigt keinen vollständigen Logout. Paketkonsistenz und gegenseitige Isolation benötigen im Anschluss den vollständigen Zwei-Benutzer-Ablauf einschließlich Fehlerfällen. Vor weiteren Builds sind zudem Erreichbarkeit des Builders und der geplante Persistenztest des Helpers zu klären.',[sources.progress,sources.brief,sources.runtime]);

// 7. The cross-device direction is explicitly a long-term target.
s=base(7,{tag:'Langfristige Vision',footer:false});
s.images.add({blob:wallpaper,contentType:'image/png',fit:'cover',position:{left:684,top:96,width:596,height:624}});
txt(s,'Ein Fundament\nfür mehrere Geräte',64,121,588,143,55,C.ink,true,{lineSpacing:1.0});
txt(s,'Smartphone und Tablet.\nNotebook und Desktop.',68,333,559,116,36,C.ink,false,{lineSpacing:1.16});
txt(s,'Ein AOSP-Kern für Android-Apps\nund GNU/Linux-Werkzeuge.',68,481,570,91,28,C.muted,false,{lineSpacing:1.12});
txt(s,'Oberflächen und Hardwareunterstützung\nsind spätere Entwicklungsaufgaben.',68,585,570,77,24,C.ink,false,{lineSpacing:1.13});
txt(s,'ARM64',737,524,479,113,89,C.ink,true,{alignment:'right'});
txt(s,'Langfristiges Hardwareziel',755,645,385,30,21,C.ink,false,{alignment:'right'});
txt(s,'07',1146,661,70,26,17,C.muted,false,{alignment:'right'});
notes(s,'Die Architektur nennt Smartphone, Tablet, ARM-Notebook und Desktop als langfristige Zielgeräte. ARM64 ist das Hardwareziel, ohne unnötige Abhängigkeit von x86-64. Phase 1 entwickelt neue Funktionen zunächst per CLI. Desktop, Launcher, grafischer Login, Smartphone-Telefonie und eigene Hardware gehören nicht zur aktuellen Phase. Diese Folie verspricht weder konkrete Geräteunterstützung noch eine funktionierende geräteübergreifende Oberfläche, Synchronisation oder Continuity. Das originale, KI-generierte Markenmotiv ist ein dekorativer Hintergrund, kein Gerät und keine Produktoberfläche.',[sources.brief,sources.progress]);

// 8. Specific areas where contributors can move the next proof forward.
s=base(8,{dark:true,tag:'Für Entwickler und frühe Anwender'});
txt(s,'Mitwirken an Aegis OS',64,122,1152,86,63,C.paper,true);
txt(s,'Die Verbindung\nentsteht im System.',68,303,589,176,54,C.lime,true,{lineSpacing:1.08});
txt(s,'Der nächste Meilenstein ist eine\nnachweisbar laufende GNU-Runtime\ninnerhalb von AOSP.',68,510,565,125,28,C.pale,false,{lineSpacing:1.12});
txt(s,'AOSP und Kernel',727,293,489,43,31,C.paper,true);
txt(s,'Produktintegration, Namespaces\nund Sicherheitsrichtlinien.',727,345,489,80,25,C.pale,false,{lineSpacing:1.13});
txt(s,'GNU/Linux und Pakete',727,453,489,43,31,C.paper,true);
txt(s,'Runtime-Lebenszyklus, Paketkonsistenz\nund reproduzierbare Systemtests.',727,505,489,81,25,C.pale,false,{lineSpacing:1.13});
const link=txt(s,'github.com/simgero/AegisOS',727,631,489,35,23,C.lime,true);
link.text.get('github.com/simgero/AegisOS').link={uri:repo,isExternal:true};
notes(s,'Die genannten Beitragsbereiche folgen aus den im Statusbericht offenen Arbeiten. Es wird kein bestehendes formales Förder-, Recruiting- oder Partnerprogramm behauptet. Der Repository-Link führt zum Entwicklungszweig codex/aosp-cloud-builder im bestätigten GitHub-Repository. Der nächste produktbezogene Beleg bleibt die echte GNU-Ausführung im integrierten AOSP-Gast, gefolgt vom vollständigen Sitzungs-, Speicher- und Paketnachweis.',[sources.progress,sources.brief,sources.runtime]);

await fs.mkdir(build+'/rendered',{recursive:true});
await (await PresentationFile.exportPptx(p)).save(build+'/candidate.pptx');
for(let i=0;i<p.slides.items.length;i++) {
  const b=await p.export({slide:p.slides.items[i],format:'png',scale:1});
  await fs.writeFile(build+'/rendered/slide-'+(i+1)+'.png',new Uint8Array(await b.arrayBuffer()));
  const layout=await p.slides.items[i].export({format:'layout'});
  await fs.writeFile(build+'/rendered/slide-'+(i+1)+'.layout.json',await layout.text());
}
await fs.writeFile(build+'/build-meta.json',JSON.stringify({font,slides:p.slides.items.length},null,2));
console.log('Created candidate and 8 slide renders.');
