"""Exercise the language selectors and scheduled screen-off through the web UI."""
import argparse
from pathlib import Path
import json
import subprocess
import sys
from playwright.sync_api import sync_playwright,expect

ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',type=Path,default=ROOT/'visuals/locales')
out=parser.parse_args().output
out.mkdir(parents=True,exist_ok=True)
messages=json.loads((ROOT/'locales/messages.json').read_text(encoding='utf-8'))
child=subprocess.Popen(['node',str(ROOT/'orca-integration/dist/test/portable-preview-fixture.js')],
    stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.DEVNULL,text=True,encoding='utf-8',
    **({'creationflags':subprocess.CREATE_NO_WINDOW} if sys.platform=='win32' else {}))
try:
    url=json.loads(child.stdout.readline())['url']
    with sync_playwright() as p:
        edge=Path('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe')
        browser=p.chromium.launch(**({'executable_path':str(edge)} if edge.exists() else {}),headless=True)
        page=browser.new_page(viewport={'width':1360,'height':1050});errors=[]
        page.on('pageerror',lambda error:errors.append(str(error)))
        page.goto(url);expect(page.locator('.quota-card')).to_have_count(5)
        home_picker=page.locator('#home-language')
        options=home_picker.locator('option')
        codes=options.evaluate_all('(options) => options.map(option => option.value)')
        assert set(codes)=={'ko',*messages['언어']},codes
        home_picker.select_option('hi')
        expect(page.locator('#notice')).to_have_text(messages['언어를 저장했습니다.']['hi'])
        page.reload();expect(page.locator('html')).to_have_attribute('lang','hi')
        page.locator('[data-page=settings]').click()
        expect(page.locator('#language')).to_have_value('hi')
        poll=page.locator('#poll')
        original_poll=poll.input_value()
        poll.fill('120')
        home_picker.select_option('ar')
        expect(page.locator('#notice')).to_have_text(messages['언어를 저장했습니다.']['ar'])
        expect(page.locator('html')).to_have_attribute('dir','rtl')
        expect(page.locator('#language')).to_have_value('ar')
        expect(poll).to_have_value('120')
        page.locator('#cancel-display').click()
        expect(poll).to_have_value(original_poll)
        for code in [*codes,'ko']:
            t=lambda key:key if code=='ko' else messages[key][code]
            page.locator('[data-page=settings]').click()
            page.locator('#language').select_option(code)
            expect(page.locator('html')).to_have_attribute('lang',code)
            expect(page.locator('html')).to_have_attribute('dir','rtl' if code in {'ar','ar-EG','ur'} else 'ltr')
            expect(page.locator('#page-title')).to_have_text(t('내게 맞는 화면으로.'))
            expect(page.locator('#account-dirty')).to_have_text(t('저장하지 않은 변경 사항이 있습니다.'))
            page.locator('#sleep-enabled').check()
            page.locator('#sleep-start').fill('23:45');page.locator('#sleep-end').fill('06:15')
            page.locator('#save-display').click()
            expect(page.locator('#notice')).to_have_text(t('설정을 저장했습니다.'))
            page.reload();expect(page.locator('html')).to_have_attribute('lang',code)
            page.locator('[data-page=settings]').click()
            expect(page.locator('#language')).to_have_value(code)
            expect(page.locator('#sleep-start')).to_have_value('23:45')
            expect(page.locator('#sleep-end')).to_have_value('06:15')
            expect(page.locator('#sleep-enabled')).to_be_checked()
            if code in {'ko','zh-CN','ja','en','de'}:
                page.screenshot(path=str(out/f'web-{code}.png'),full_page=True)
            page.locator('#sleep-end').fill('23:45');page.locator('#save-display').click()
            expect(page.locator('#notice')).to_have_text(t('시작과 종료 시각은 달라야 합니다.'))
            page.locator('#cancel-display').click()
            expect(page.locator('#sleep-end')).to_have_value('06:15')
            page.locator('#sleep-start').fill('24:99');page.locator('#save-display').click()
            expect(page.locator('#notice')).to_have_text(t('시간은 24시간제 HH:MM 형식으로 입력하세요.'))
            page.locator('#cancel-display').click()
            expect(page.locator('#sleep-start')).to_have_value('23:45')
            # UI strings must switch in every page, with no cross-language
            # leftovers. Account names/plans are user data and are preserved.
            for name in ['overview','accounts','device','settings']:
                page.locator(f'[data-page={name}]').click()
                if code!='ko':
                    remaining=page.evaluate('''keys => {
                      const walker=document.createTreeWalker(document.body,NodeFilter.SHOW_TEXT), found=[];
                      while(walker.nextNode()) {
                        const node=walker.currentNode, text=node.textContent.trim().replace(/\\s+/g,' ');
                        if(node.parentElement.checkVisibility() && keys.includes(text))found.push(text);
                      }return found;
                    }''',[key for key,row in messages.items() if row[code]!=key])
                    assert not remaining,(code,name,remaining)
            page.set_viewport_size({'width':430,'height':1000})
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),code
            page.set_viewport_size({'width':1360,'height':1050})
        assert not errors,errors
        browser.close()
    result={'passed':True,'languages':codes,'mock_source':True,'mock_ble':True,
        'checks':['home selection, persistence and draft preservation','RTL','instant preview','save and reload','all pages translated','overnight schedule persistence',
                  'equal-time and malformed-time rejection','cancel restores settings','430px layout','no JavaScript errors']}
    (out/'web-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(f'{len(codes)}-language web UI and screen schedule: PASS')
finally:
    child.stdin.close()
    try:child.wait(timeout=10)
    except subprocess.TimeoutExpired:child.kill();child.wait()
