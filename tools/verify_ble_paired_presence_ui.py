"""Verify the actual portable UI using simulated paired BLE telemetry.

Requires an already built orca-integration fixture. No physical BLE operations.
"""
from pathlib import Path
import json
import subprocess
import sys
from playwright.sync_api import sync_playwright, expect

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / '.local/stage-d-web'
out.mkdir(parents=True, exist_ok=True)
messages = json.loads((ROOT / 'locales/messages.json').read_text(encoding='utf-8'))
child = subprocess.Popen(['node', str(ROOT / 'orca-integration/dist/test/portable-preview-fixture.js')],
    stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, encoding='utf-8',
    creationflags=subprocess.CREATE_NO_WINDOW if sys.platform == 'win32' else 0)
def emit(mode):
    child.stdin.write('presence-' + mode + '\n')
    child.stdin.flush()

try:
    url = json.loads(child.stdout.readline())['url']
    with sync_playwright() as p:
        edge = Path('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe')
        browser = p.chromium.launch(**({'executable_path':str(edge)} if edge.exists() else {}), headless=True)
        page = browser.new_page(viewport={'width':1360, 'height':1050})
        errors = []
        page.on('pageerror', lambda error: errors.append(str(error)))
        page.goto(url)
        expect(page.locator('.quota-card')).to_have_count(5)
        page.locator('[data-page=device]').click()
        page.locator('#scan').click()
        page.locator('#devices button').first.click()
        expect(page.locator('#pair-dialog')).to_be_visible()
        page.locator('#accept-pin').click()
        expect(page.locator('#device-link')).to_have_text('연결됨')
        page.locator('[data-page=settings]').click()
        expect(page.locator('#presence-status')).to_have_text('연결된 보드는 휴대폰 상태 표시를 지원하지 않습니다.')
        for obsolete in ['presence-scan', 'presence-devices', 'presence-address',
                         'presence-name', 'presence-aliases', 'presence-enabled']:
            expect(page.locator('#' + obsolete)).to_have_count(0)
        assert page.evaluate("""async () => (await fetch('/api/presence-scan', {
            method:'POST', headers:{'X-Quotamon-Key':sessionStorage.getItem('quotamon-key'),
            'Content-Type':'application/json'},body:'{}'})).status""") == 404
        for code in ['ko', 'zh-CN', 'ja', 'en', 'de']:
            t = lambda key: key if code == 'ko' else messages[key][code]
            page.locator('#language').select_option(code)
            page.locator('#save-display').click()
            expect(page.locator('#notice')).to_have_text(t('설정을 저장했습니다.'))
            page.reload()
            expect(page.locator('html')).to_have_attribute('lang', code)
            page.locator('[data-page=settings]').click()
            emit('connected')
            expect(page.locator('#presence-status')).to_have_text(t('휴대폰 연결됨'))
            expect(page.locator('#presence-phone-name')).to_have_text(t('등록된 휴대폰'))
            emit('disconnected_grace')
            expect(page.locator('#presence-status')).to_have_text(t('재연결 대기 · %d초 후 화면 끄기').replace('%d', '42'))
            page.screenshot(path=str(out / f'web-{code}.png'), full_page=True)
            emit('absent')
            expect(page.locator('#presence-status')).to_have_text(t('휴대폰 부재 · 화면 끄기'))
            emit('unavailable')
            expect(page.locator('#presence-status')).to_have_text(t('Bluetooth 오류'))
            emit('html')
            expect(page.locator('#presence-phone-name')).to_have_text('<img src=x onerror=bad>')
            expect(page.locator('#presence-phone-name img')).to_have_count(0)
            page.set_viewport_size({'width':430, 'height':1000})
            assert page.evaluate('document.documentElement.scrollWidth <= innerWidth'), code
            page.set_viewport_size({'width':1360, 'height':1050})
            page.locator('#brightness').fill('80')
            page.locator('#cancel-display').click()
            expect(page.locator('#brightness')).to_have_value('70')
        assert not errors, errors
        browser.close()
    (out / 'checks.json').write_text(json.dumps({'passed':True, 'mock_ble':True,
        'hardware_tested':False, 'languages':['ko','zh-CN','ja','en','de'],
        'checks':['read-only paired status', 'no retired phone scan controls or endpoint',
                  'unsupported old board', 'connected/grace/absent/unavailable',
                  'literal HTML name', 'save/reload/cancel', '430px layout', 'no JavaScript errors']}, indent=2)+'\n', encoding='utf-8')
    print('Paired BLE portable UI: PASS (simulated telemetry, five languages).')
finally:
    child.stdin.close()
    try:
        child.wait(timeout=10)
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait()
