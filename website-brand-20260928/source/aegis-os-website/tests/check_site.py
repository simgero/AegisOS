from pathlib import Path
from playwright.sync_api import sync_playwright
import json, os

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'tests'
reports = []
HTML = (ROOT / 'index.html').read_text(encoding='utf-8')
import base64
wallpaper = 'data:image/svg+xml;base64,' + base64.b64encode((ROOT / 'assets/wallpaper.svg').read_bytes()).decode()
HTML = HTML.replace("url('assets/wallpaper.svg')", f"url('{wallpaper}')")
with sync_playwright() as p:
    browser_path = os.environ.get('AEGIS_CHROMIUM') or ('/usr/bin/chromium' if Path('/usr/bin/chromium').exists() else None)
    browser = p.chromium.launch(executable_path=browser_path, headless=True, args=['--no-sandbox', '--disable-dev-shm-usage'])
    for width, height, name in [(1440,1000,'desktop'),(390,844,'mobile'),(768,1024,'tablet'),(320,750,'small-mobile')]:
        page = browser.new_page(viewport={'width':width, 'height':height}, device_scale_factor=1)
        errors=[]
        requests=[]
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.on('request', lambda r: requests.append(r.url))
        page.set_content(HTML, wait_until='load')
        page.wait_for_timeout(1300)
        page.screenshot(path=str(OUT / f'{name}-hero.png'), animations='disabled')
        page.evaluate('document.documentElement.style.scrollBehavior="auto"')
        total = page.evaluate('document.documentElement.scrollHeight')
        for y in range(0,total,650):
            page.evaluate('(y)=>window.scrollTo(0,y)',y)
            page.wait_for_timeout(90)
        page.wait_for_timeout(900)
        page.evaluate('window.scrollTo({top:0,behavior:"instant"})')
        page.wait_for_timeout(500)
        if name in ['desktop','mobile']:
            page.screenshot(path=str(OUT / f'{name}-full.png'),full_page=True, animations='disabled')
        overflow = page.evaluate('({scroll:document.documentElement.scrollWidth, viewport:innerWidth})')
        # Every internal anchor must refer to an existing target.
        bad_anchors=page.evaluate('''Array.from(document.querySelectorAll('a[href^="#"]')).filter(a=>a.hash&& !document.getElementById(a.hash.slice(1))).map(a=>a.outerHTML)''')
        assert not errors, (name,errors)
        assert overflow['scroll']<=width, (name,overflow)
        assert not bad_anchors, (name,bad_anchors)
        # Interactive profile switch.
        page.get_by_role('button',name='Sam',exact=True).click()
        assert page.locator('#profile-name').inner_text()=='Sams persönlicher Bereich'
        assert page.get_by_role('button',name='Sam',exact=True).get_attribute('aria-pressed')=='true'
        assert page.locator('#profile-files').inner_text().find('Reisepläne')>=0
        page.get_by_role('button',name='Alex',exact=True).click()
        # Four roadmap phases, keyboard operation, ARIA focus state.
        for i in range(4):
            page.locator(f'#phase-tab-{i}').click()
            assert page.locator(f'#phase-tab-{i}').get_attribute('aria-selected')=='true'
            assert page.locator('#roadmap-panel').get_attribute('aria-labelledby')==f'phase-tab-{i}'
        page.locator('#phase-tab-3').press('Home')
        assert page.locator('#phase-tab-0').get_attribute('aria-selected')=='true'
        page.locator('#phase-tab-0').press('ArrowRight')
        assert page.locator('#phase-tab-1').get_attribute('aria-selected')=='true'
        # FAQs.
        faq=page.locator('.faq-item').nth(1)
        faq.locator('summary').click()
        assert faq.get_attribute('open') is not None
        faq.locator('summary').click()
        # Native dialogs, downloadable briefings, return focus.
        page.locator('[data-dialog="developers"]').click()
        assert page.locator('#info-dialog').evaluate('(el)=>el.open')
        with page.expect_download() as info:
            page.get_by_role('button',name='Briefing herunterladen').click()
        download = info.value
        assert download.suggested_filename=='Aegis-OS-Entwicklungsbriefing.md'
        download.save_as(str(OUT/'developer-briefing-test.md'))
        assert 'keine Daten' in page.locator('#download-note').inner_text()
        page.keyboard.press('Escape')
        assert not page.locator('#info-dialog').evaluate('(el)=>el.open')
        for key in ['sponsors','privacy-vision','status','site-privacy','imprint']:
            page.locator(f'[data-dialog="{key}"]').first.click()
            assert page.locator('#info-dialog').evaluate('(el)=>el.open')
            page.get_by_role('button',name='Dialog schließen').click()
        if width<=620:
            page.evaluate('window.scrollTo({top:0,behavior:"instant"})')
            page.get_by_role('button',name='Menü öffnen').click()
            assert page.locator('#main-nav').evaluate('(el)=>el.classList.contains("is-open")')
            page.get_by_role('navigation').get_by_role('link',name='Für dich',exact=True).click()
            assert page.locator('.menu-toggle').get_attribute('aria-expanded')=='false'
        external = [r for r in requests if r.startswith('http') and not r.startswith('http://127.0.0.1:8765/')]
        assert not external, external
        reports.append({'viewport':name,'dimensions':[width,height],'scroll_width':overflow['scroll'],'page_height':total,'javascript_errors':errors,'external_requests':external,'profile_switch':'passed','roadmap_mouse_keyboard':'passed','faq':'passed','dialogs':'passed','download':'passed','mobile_navigation':'passed' if width<=620 else 'n/a'})
        page.close()
    # Reading without JavaScript and reduced-motion support.
    page=browser.new_page(viewport={'width':390,'height':844},java_script_enabled=False)
    page.set_content(HTML)
    assert page.locator('h1').is_visible()
    assert page.locator('#faq-heading').count()==1
    page.close()
    page=browser.new_page(viewport={'width':1440,'height':1000},reduced_motion='reduce')
    page.set_content(HTML)
    assert page.locator('.reveal.pending').count()==0
    page.close()
    browser.close()
(OUT/'test-report.json').write_text(json.dumps({'checks':reports,'no_javascript_reading':'passed','reduced_motion':'passed'},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(reports,ensure_ascii=False,indent=2))
