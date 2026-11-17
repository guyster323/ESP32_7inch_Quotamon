"""Headless browser checks of the real settings UI with a simulated source/radio."""
from pathlib import Path
import json,subprocess,sys
from playwright.sync_api import sync_playwright,expect

root=Path(__file__).resolve().parents[1]
out=root/'visuals'/'portable';out.mkdir(parents=True,exist_ok=True)
child=subprocess.Popen(['node',str(root/'orca-integration/dist/test/portable-preview-fixture.js')],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8',
    **({'creationflags':subprocess.CREATE_NO_WINDOW} if sys.platform=='win32' else {}))
try:
    url=json.loads(child.stdout.readline())['url']
    with sync_playwright() as p:
        edge=Path('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe')
        browser=p.chromium.launch(**({'executable_path':str(edge)} if edge.exists() else {}),headless=True)
        page=browser.new_page(viewport={'width':1360,'height':1000});errors=[];state_requests=[]
        page.on('request',lambda request:state_requests.append(request.url) if request.url.endswith('/api/state') else None)
        page.on('pageerror',lambda error:errors.append(str(error)))
        page.on('console',lambda message:errors.append(message.text) if message.type=='error' else None)
        page.goto(url);expect(page.locator('.quota-card')).to_have_count(5)
        cursor=page.locator('.quota-card').filter(has=page.locator('.provider',has_text='cursor'))
        expect(cursor.locator('.quota-pool')).to_have_text(['Cursor Models','Other Models'])
        expect(cursor.locator('.quota-number')).to_have_text(['82%','47%'])
        expect(cursor.locator('.bar')).to_have_count(2)
        expect(cursor).not_to_contain_text('MONTHLY')
        codex=page.locator('.quota-card').filter(has=page.locator('.provider',has_text='codex'))
        expect(codex.locator('.reset-label')).to_have_text(['R.T.1','R.T.2'])
        expect(codex.locator('.reset-credit')).not_to_contain_text(['UTC','UTC'])
        expect(codex.locator('.reset-credit .reset-absolute')).to_have_count(0)
        copilot=page.locator('.quota-card').filter(has=page.locator('.provider',has_text='copilot'))
        expect(copilot.locator('.card-note')).to_contain_text('Plan Reset')
        expect(copilot).not_to_contain_text('AIC')
        expect(cursor.locator('.card-note')).to_have_count(1)
        expect(cursor.locator('.card-note')).to_contain_text('Plan Reset M : ')
        expect(copilot.locator('.card-note')).to_contain_text('Plan Reset M : ')
        assert page.evaluate('''() => {
          const max = { ...state.accounts.find(a => a.provider === 'codex'), enabled: true,
            pools: [{ name: '5H', reset: '5h' }, { name: 'WEEKLY', reset: '7d' }] }
          const row = planSchedule(max, max.pools[0])
          if (row.textContent !== 'Plan 5h : 5h | W 7d') return false
          if (![...row.querySelectorAll('span')].every(s => s.style.color === 'rgb(255, 129, 127)')) return false
          max.pools[0].reset = '0m'
          max.pools[1].reset = '3d 12h'
          const mixed = planSchedule(max, max.pools[0]).querySelectorAll('span')
          if (mixed[0].style.color !== 'rgb(25, 215, 156)' || mixed[1].style.color !== 'rgb(140, 172, 142)') return false
          for (const provider of ['cursor', 'copilot']) {
            for (const [reset, colour] of [['30d', 'rgb(255, 129, 127)'], ['0m', 'rgb(25, 215, 156)']]) {
              const part = planSchedule({ provider, pools: [], enabled: true }, { reset }).firstChild
              if (part.textContent !== 'Plan Reset M : ' + reset || part.style.color !== colour) return false
            }
          }
          if (creditLine({expires_at: Date.now()/1000 - 60}, true).style.color !== 'rgb(255, 129, 127)') return false
          if (creditLine({expires_at: Date.now()/1000 + 15*86400}, false).style.color !== 'rgb(25, 215, 156)') return false
          return countdownColor(-1, 300) === '#8ea8a8'
        }'''), 'Countdown labels, ranges or individual segment colours differ from the device'
        idle_requests=len(state_requests)
        page.wait_for_timeout(2300)
        assert len(state_requests)==idle_requests,'Idle UI must not poll every two seconds'
        page.screenshot(path=str(out/'01-dashboard-mock.png'),full_page=True)
        page.get_by_role('button',name='계정 선택',exact=True).click()
        expect(page.locator('.account-row')).to_have_count(5)
        row=page.locator('.account-row').first
        row.locator('input[type=checkbox]').check()
        row.get_by_role('textbox').first.fill('Business Premium')
        page.wait_for_timeout(2300)
        expect(row.get_by_role('textbox').first).to_have_value('Business Premium')
        page.get_by_role('button',name='계정 설정 저장').click()
        expect(page.get_by_role('status')).to_contain_text('설정을 저장')
        page.screenshot(path=str(out/'02-accounts-mock.png'),full_page=True)
        page.get_by_role('button',name='디스플레이 연결',exact=True).click()
        page.get_by_role('button',name='디스플레이 찾기',exact=True).click()
        page.get_by_role('button',name='연결',exact=True).click()
        expect(page.get_by_role('dialog')).to_be_visible()
        expect(page.locator('#pin')).to_have_text('632814')
        page.screenshot(path=str(out/'03-pairing-mock.png'),full_page=True)
        page.get_by_role('button',name='같은 번호 · 승인',exact=True).click()
        expect(page.locator('#device-link')).to_have_text('연결됨')
        page.get_by_role('button',name='설정',exact=True).click()
        expect(page.locator('#show-credits, #show-session')).to_have_count(0)
        page.locator('#poll').fill('90');page.locator('#brightness').fill('60')
        page.get_by_role('button',name='디스플레이 설정 저장').click()
        expect(page.get_by_role('status')).to_contain_text('설정을 저장')
        expect(page.locator('#poll')).to_have_value('90')
        page.screenshot(path=str(out/'04-settings-mock.png'),full_page=True)
        page.locator('#poll').fill('120')
        page.get_by_role('button',name='취소 · 최신 설정 읽기',exact=True).click()
        expect(page.locator('#poll')).to_have_value('90')
        page.get_by_role('button',name='디스플레이 연결',exact=True).click()
        page.get_by_role('button',name='자동 연결 해제',exact=True).click()
        expect(page.locator('#device-link')).to_have_text('연결 대기')
        page.get_by_role('button',name='연결',exact=True).click()
        expect(page.get_by_role('dialog')).to_be_visible()
        page.get_by_role('button',name='거절',exact=True).click()
        expect(page.get_by_role('dialog')).not_to_be_visible()
        expect(page.locator('#device-link')).to_have_text('연결 대기')
        # Authenticated local UI token survives an ordinary page refresh.
        page.reload();expect(page.locator('.quota-card')).to_have_count(5)
        page.set_viewport_size({'width':760,'height':1000})
        page.screenshot(path=str(out/'05-narrow-mock.png'),full_page=True)
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
        page.set_viewport_size({'width':430,'height':1000})
        page.get_by_role('button',name='계정 선택',exact=True).click()
        assert page.evaluate('document.documentElement.scrollWidth<=innerWidth')
        child.stdin.write('offline\n');child.stdin.flush()
        page.get_by_role('button',name='대시보드',exact=True).click()
        expect(page.locator('.quota-card')).to_have_count(0)
        expect(page.locator('#empty')).to_be_visible()
        assert not errors,errors
        browser.close()
    checks=dict(passed=True,actual_product_ui=True,mock_source=True,mock_ble=True,hardware_verified=False,
        checks=['five-provider assets','Cursor shows both model budgets without Monthly','no two-second idle polling','selection and aliases survive polling','settings save',
        'numeric pairing approval/rejection while connect pending','page refresh keeps local UI access',
        '760/430px no overflow','Orca loss removes cards','no browser/CSP errors'])
    (out/'checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print('Portable product UI: PASS; source and BLE are simulated.')
finally:
    child.stdin.close()
    try:child.wait(timeout=10)
    except subprocess.TimeoutExpired:child.kill();child.wait()
