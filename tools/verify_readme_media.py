"""Verify committed localized GIF/MP4 demos and their actual browser playback."""
from pathlib import Path
from functools import partial
from http.server import ThreadingHTTPServer,SimpleHTTPRequestHandler
from threading import Thread
import hashlib
import io
import json
import re
import struct
from PIL import Image,ImageChops,ImageStat
from playwright.sync_api import sync_playwright,expect

ROOT=Path(__file__).resolve().parents[1]
folder=ROOT/'assets/readme'
metadata=json.loads((folder/'media.json').read_text())
locales=json.loads((ROOT/'tools/readme_media_locales.json').read_text(encoding='utf8'))
assert set(metadata['languages'])==set(locales)
assert metadata['duration_ms']==24000 and metadata['live_account_data_used'] is False
for name,entry in metadata['files'].items():
    path=folder/name
    assert path.stat().st_size==entry['bytes']
    assert hashlib.sha256(path.read_bytes()).hexdigest()==entry['sha256'],name
report={'passed':True,'mock_data':True,'hardware_footage':False,'languages':{},
    'checks':['file hashes','24-second looping GIFs','native character pixels move','screen-off and wake frames',
              'H.264 fast-start MP4','five-language browser playback','language switch','restart','430px layout','no JavaScript errors']}
for code,variant in metadata['languages'].items():
    base=folder/variant['base_path']
    with Image.open(base/'quotamon-demo.gif') as gif:
        assert gif.size==(1200,900) and gif.info['loop']==0
        samples={};duration=0
        for index in range(gif.n_frames):
            gif.seek(index);duration+=gif.info['duration']
            if index in [0,10,20,30,40,210,230]:samples[index]=gif.convert('RGB')
        assert gif.n_frames==240 and duration==24000,(code,gif.n_frames,duration)
    crop=(220,350,995,705)
    assert any(ImageChops.difference(samples[0].crop(crop),samples[n].crop(crop)).getbbox() for n in [10,20,30,40]),code
    assert max(high for low,high in samples[210].crop((200,276,1000,756)).getextrema())<6,code
    assert ImageStat.Stat(samples[230].crop(crop)).stddev[0]>10,code
    binary=(base/'quotamon-demo.mp4').read_bytes();offset=0;atoms=[]
    while offset<len(binary):
        size,kind=struct.unpack('>I4s',binary[offset:offset+8]);header=8
        if size==1:size=struct.unpack('>Q',binary[offset+8:offset+16])[0];header=16
        if size==0:size=len(binary)-offset
        assert size>=header
        atoms.append(kind);offset+=size
    assert atoms.index(b'moov')<atoms.index(b'mdat') and b'avc1' in binary,code
    assert variant['native_checks']['passed'] and variant['native_checks']['mock_data']
    report['languages'][code]={'gif_frames':240,'duration_ms':duration,'animated_native_pixels':True,'browser_playback':False}
for name in ['README.md','README.en.md','docs/README_MEDIA.md']:
    path=ROOT/name
    for target in re.findall(r'\]\(([^)]+)\)|(?:href|src)="([^"]+)"',path.read_text(encoding='utf8')):
        link=next(x for x in target if x).split('#')[0]
        if link and '://' not in link:assert (path.parent/link).exists(),(name,link)

class Quiet(SimpleHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_GET(self):
        # Python's basic handler has no byte-range support. Chromium otherwise
        # advertises a [0,0] seekable range even after buffering the full MP4.
        if not self.path.split('?')[0].endswith('.mp4'):
            return super().do_GET()
        path=Path(self.translate_path(self.path))
        if not path.is_file():return self.send_error(404)
        total=path.stat().st_size;start,end=0,total-1
        requested=self.headers.get('Range')
        if requested:
            match=re.fullmatch(r'bytes=(\d*)-(\d*)',requested)
            if not match:return self.send_error(416)
            a,b=match.groups()
            if a:start=int(a);end=min(int(b),end) if b else end
            elif b:start=max(0,total-int(b))
            if start>end:return self.send_error(416)
        self.send_response(206 if requested else 200)
        self.send_header('Content-Type','video/mp4');self.send_header('Accept-Ranges','bytes')
        self.send_header('Content-Length',str(end-start+1))
        if requested:self.send_header('Content-Range',f'bytes {start}-{end}/{total}')
        self.end_headers()
        with path.open('rb') as stream:
            stream.seek(start);remaining=end-start+1
            while remaining:
                block=stream.read(min(65536,remaining))
                if not block:break
                try:self.wfile.write(block)
                except (BrokenPipeError,ConnectionResetError):break
                remaining-=len(block)
server=ThreadingHTTPServer(('127.0.0.1',0),partial(Quiet,directory=str(ROOT)))
Thread(target=server.serve_forever,daemon=True).start()
work=ROOT/'.local/readme-media';work.mkdir(parents=True,exist_ok=True)
try:
    with sync_playwright() as p:
        edge=Path('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe')
        browser=p.chromium.launch(**({'executable_path':str(edge)} if edge.exists() else {}),headless=True)
        page=browser.new_page(viewport={'width':1280,'height':1150});errors=[]
        page.on('pageerror',lambda error:errors.append(str(error)))
        for code,copy in locales.items():
            page.goto(f'http://127.0.0.1:{server.server_port}/assets/readme/index.html?lang={code}')
            expect(page.locator('html')).to_have_attribute('lang',code)
            expect(page.locator('h1')).to_have_text(copy['title'])
            page.wait_for_function('demo.readyState>=2 && !demo.paused && demo.currentTime>.1')
            video=page.locator('video');duration=video.evaluate('(v)=>v.duration')
            assert 23.8<=duration<=24.2,(code,duration)
            assert video.evaluate('(v)=>v.loop && v.muted')
            video.evaluate('(v)=>{v.pause();v.currentTime=.4}')
            page.wait_for_function('!demo.seeking && Math.abs(demo.currentTime-.4)<.1')
            before=Image.open(io.BytesIO(video.screenshot())).convert('RGB').resize((1200,900))
            video.evaluate('(v)=>v.currentTime=3.6')
            page.wait_for_function('!demo.seeking && Math.abs(demo.currentTime-3.6)<.1')
            after=Image.open(io.BytesIO(video.screenshot())).convert('RGB').resize((1200,900))
            assert ImageChops.difference(before.crop((230,360,990,700)),after.crop((230,360,990,700))).getbbox(),code
            page.get_by_role('button',name=copy['restart'],exact=True).click()
            page.wait_for_function('!demo.paused && demo.currentTime>.1 && demo.currentTime<2')
            expect(page.get_by_role('status')).to_have_text(copy['playing'])
            page.screenshot(path=str(work/f'player-{code}.png'),full_page=True)
            page.set_viewport_size({'width':430,'height':1000})
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),code
            page.set_viewport_size({'width':1280,'height':1150})
            next_code=list(locales)[(list(locales).index(code)+1)%5]
            page.locator('#language').select_option(next_code)
            expect(page.locator('html')).to_have_attribute('lang',next_code)
            page.wait_for_function('demo.readyState>=2 && !demo.paused && demo.currentTime>.1')
            assert page.locator('#mp4').get_attribute('href')==metadata['languages'][next_code]['base_path']+'quotamon-demo.mp4'
            report['languages'][code]['browser_playback']=True
        assert not errors,errors
        browser.close()
finally:
    server.shutdown();server.server_close()
(folder/'checks.json').write_text(json.dumps(report,indent=2)+'\n')
print('Five-language animated GIFs, H.264 MP4 playback, player controls, README links and mobile layout: PASS')
