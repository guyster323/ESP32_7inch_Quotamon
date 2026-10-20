"""Extend the existing Korean/Latin fonts with every localized LCD UI glyph.

Requires fontTools, lv_font_conv and the reviewed Noto Sans KR Medium font.
Noto Sans SC supplies simplified characters absent from the Korean font.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import urllib.request

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'.tools/python'))
from fontTools.ttLib import TTFont
from fontTools.varLib.instancer import instantiateVariableFont

folder=ROOT/'.tools/fonts'
sc=folder/'NotoSansSC.ttf'
pin='2894aab31764f10f29c421bdfd2340d3b382d384'
expected='a3041811a78c361b1de50f953c805e0244951c21c5bd412f7232ef0d899af0da'
for name,remote in [('NotoSansSC.ttf','NotoSansSC%5Bwght%5D.ttf'),('NotoSansSC-OFL.txt','OFL.txt')]:
    path=folder/name
    if not path.exists():
        with urllib.request.urlopen(f'https://raw.githubusercontent.com/google/fonts/{pin}/ofl/notosanssc/{remote}',timeout=60) as response:path.write_bytes(response.read())
assert hashlib.sha256(sc.read_bytes()).hexdigest()==expected,'Unreviewed SC font'
kr=folder/'NotoSansKR-Medium.ttf'
assert kr.exists(),'Generate the existing Noto Sans KR Medium font first'
text=''.join((ROOT/p).read_text(encoding='utf-8') for p in ['native/ui.cpp','native/bluetooth_name.h','firmware/main/main.cpp','native/generated/translations.h'])
characters={c for c in text if ord(c)>127}
krmap=TTFont(kr).getBestCmap();scmap=TTFont(sc).getBestCmap()
missing=characters-set(map(chr,krmap))
assert all(ord(c) in scmap for c in missing),f'Missing glyphs: {sorted(c for c in missing if ord(c) not in scmap)}'
static=folder/'NotoSansSC-Medium.ttf'
if not static.exists():instantiateVariableFont(TTFont(sc),{'wght':500},inplace=True).save(static)
binary=ROOT/'.tools/fontconv/node_modules/lv_font_conv/lv_font_conv.js'
for size in [14,16,20]:
    # Keep all UI glyphs at their intended sizes, but share the full Hangul
    # alphabet at 16px rather than storing it three times in XIP PSRAM.
    alphabet='0x20-0x7e,0xa0-0x24f,0x25a1=>0xfffd'+(',0xac00-0xd7a3' if size==16 else '')
    args=['node',str(binary),'--size',str(size),'--bpp','4','--format','lvgl',
          '--font',str(kr),'--range',alphabet,
          '--symbols',''.join(sorted(characters-missing))]
    if missing:args+=['--font',str(static),'--symbols',''.join(sorted(missing))]
    args+=['--lv-fallback','font_cjk16' if size==16 else 'font_ko16','-o',str(ROOT/f'native/generated/font_ko{size}.c')]
    subprocess.run(args,check=True,cwd=ROOT)
# A single shared 16px fallback covers arbitrary BMP Chinese/Japanese device
# names. Keep the existing 4bpp UI glyphs; 2bpp compressed fallback saves flash
# and XIP PSRAM. Only assigned characters present in the reviewed fonts are used.
wanted=set(range(0x3400,0x4dc0))|set(range(0x4e00,0xa000))|set(range(0x3040,0x3100))|set(range(0x31f0,0x3200))|set(range(0xff00,0xfff0))|set(range(0x2000,0x2070))|set(range(0x3000,0x3040))
def ranges(codes):
    spans=[]
    for code in sorted(codes):
        if spans and code==spans[-1][1]+1:spans[-1][1]=code
        else:spans.append([code,code])
    return ','.join(f'0x{a:x}-0x{b:x}' if a!=b else f'0x{a:x}' for a,b in spans)
cjk=wanted&scmap.keys()
extra=(wanted&krmap.keys())-cjk
args=['node',str(binary),'--size','16','--bpp','2','--format','lvgl',
      '--font',str(static),'--range',ranges(cjk)]
if extra:args+=['--font',str(kr),'--range',ranges(extra)]
args+=['--no-kerning','-o',str(ROOT/'native/generated/font_cjk16.c')]
subprocess.run(args,check=True,cwd=ROOT)
shutil.copyfile(folder/'NotoSansSC-OFL.txt',ROOT/'native/generated/NotoSansSC-OFL.txt')
path=ROOT/'native/generated/asset-manifest.json';manifest=json.loads(path.read_text(encoding='utf-8'))
manifest['ui_languages']={'codes':['ko','zh-CN','ja','en','de'],'catalog':'locales/messages.json',
    'font_sizes':[14,16,20],'additional_font':'Noto Sans SC Medium (weight 500)',
    'source':f'https://github.com/google/fonts/tree/{pin}/ofl/notosanssc','source_sha256':expected,
    'additional_symbols':''.join(sorted(characters)),'fallback_symbols':''.join(sorted(missing)),
    'coverage':'All translated LCD UI text, modern Hangul, ASCII and Latin extended; shared compressed 16px fallback for BMP CJK unified/Extension A ideographs, kana, punctuation and fullwidth characters supported by Noto Sans SC/KR.',
    'cjk_fallback':{'font':'font_cjk16','size':16,'bpp':2,'compressed':True,'glyphs':len(cjk|extra)},
    'ui_compressed':True,'shared_hangul_size':16}
path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(f'Localized UI fonts: {len(characters)} non-ASCII symbols, {len(missing)} simplified fallback glyphs.')
