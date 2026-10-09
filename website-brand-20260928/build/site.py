from pathlib import Path
import re,json
base=Path('/Users/simeongerodetti/AegisOS/website-brand-20260928')
s=(base/'source/aegis-os-website/index.html').read_text()
s=re.sub(r'<style>.*?</style>','<link rel="stylesheet" href="style.css">',s,flags=re.S)
start=s.index('  <div class="device-stage"');end=s.index(' </section>',start)
s=s[:start]+'''<div class="hero-meta wrap"><span>01 / DIE VISION</span><span>Smartphone · Tablet · Computer</span><span>Architekturphase / 2026</span></div>'''+s[end:]
s=s.replace('<section class="hero" aria-labelledby="hero-heading">','<section class="hero" aria-labelledby="hero-heading"><img class="hero-art" src="assets/aegis-key-visual.png" alt="" fetchpriority="high">')
s=s.replace('Eine neue Idee von Unabhängigkeit · In Entwicklung','UNABHÄNGIGKEIT BEGINNT AUF DEINEM GERÄT')
s=s.replace('Dein digitales Leben.<br><span class="soft">Wieder deins.</span>','Dein Leben.<br>Deine Daten.<br><span class="soft">Dein Aegis.</span>')
s=s.replace('Wir entwickeln Aegis OS. Ein Betriebssystem, das deine Privatsphäre zum Ausgangspunkt macht. Und dich zum Mittelpunkt.','Die Vision eines Betriebssystems, das dir die Kontrolle zurückgibt. Lokal gedacht. Mit Privatsphäre als Grundlage.')
s=s.replace('<div class="hero-actions">','<div class="hero-actions">')
start=s.index('<div class="privacy-visual"');end=s.index('</section>',start)
s=s[:start]+'''<div class="privacy-statement"><span class="large-index">A / 01</span><p>Dein Zugang bleibt<br><em>dein Zugang.</em></p><span>Lokale Identität als Produktziel</span></div></div></div><div class="privacy-footnote">Entwicklungsziel: lokale Anmeldung, getrennte Daten und überprüfbare Zugriffssperren.</div>\n '''+s[end:]
s=re.sub(r'<div class="dev-art".*?</div><button', '<button',s,flags=re.S)
s=re.sub(r'<div class="sponsor-art".*?</div><button', '<button',s,flags=re.S)
s=s.replace('Hier wird nichts getrackt.','Datenschutz dieser Vorschau')
s=s.replace('Es werden keine persönlichen Angaben abgefragt oder an einen Server übermittelt.','Die Seite enthält kein Kontaktformular. Beim Aufruf verarbeitet der Hostinganbieter die technisch erforderlichen Verbindungsdaten; der private Zugang kann eine Anmeldung erfordern.')
s=s.replace('keine Analysewerkzeuge, Werbetracker, externen Schriftarten oder Cookies','keine eingebundenen Analysewerkzeuge, Werbetracker oder extern geladenen Schriftarten')
s=s.replace('Nicht veröffentlichte Projektvorschau','Private Projektvorschau')
s=s.replace('Geräteabbildungen und Benutzeroberflächen sind Designstudien.','Gezeigte Oberflächen sind Designstudien.')
s=s.replace('content="light"','content="dark light"').replace('content="#f5f6f8"','content="#090b10"')
s=s.replace('<title>Aegis OS — Dein digitales Leben. Wieder deins.</title>','<title>Aegis OS — Dein Leben. Deine Daten. Dein Aegis.</title>')
s=s.replace('<section class="closing"','<section class="downloads wrap"><p class="eyebrow">AEGIS OS / ZUM MITNEHMEN</p><h2>Die Idee im Detail.</h2><div><a href="downloads/Aegis-OS-Praesentation.pptx" download>Präsentation herunterladen ↗</a><a href="downloads/Aegis-OS-Brand-Assets.zip" download>Logo &amp; Brand Assets ↗</a></div></section><section class="closing"')
(base/'site/dist/index.html').write_text(s)
m=base/'site/.openai/hosting.json';j=json.loads(m.read_text());j['static']={'directory':'dist'};m.write_text(json.dumps(j,indent=2))
a=base/'output/brand-assets';a.mkdir(parents=True,exist_ok=True)
# Refine and package the original vector A identity rather than inventing unrelated artwork.
for name,color in [('light','#F5F7FA'),('dark','#090B10'),('blue','#526EFF')]:
 mark=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><path d="M10 54 32 10l22 44M22 40h20" fill="none" stroke="{color}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/></svg>'
 (a/f'aegis-symbol-{name}.svg').write_text(mark)
 lock=f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 310 72"><path d="M10 58 32 14l22 44M22 44h20" fill="none" stroke="{color}" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"/><text x="78" y="55" fill="{color}" font-family="Arial,Helvetica,sans-serif" font-size="52" font-weight="700" letter-spacing="-2">aegis</text><text x="222" y="54" fill="{color}" font-family="Arial,Helvetica,sans-serif" font-size="24" letter-spacing="3">OS</text></svg>'
 (a/f'aegis-logo-{name}.svg').write_text(lock)
(a/'brand-tokens.json').write_text(json.dumps({'name':'Aegis OS','colors':{'graphite':'#090B10','paper':'#F5F7FA','electric':'#526EFF','muted':'#A5ACBA'},'font':'Arial, Helvetica, sans-serif','tagline':'Dein Leben. Deine Daten. Dein Aegis.'},indent=2))
