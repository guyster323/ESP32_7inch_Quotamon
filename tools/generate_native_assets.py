"""Convert MIT F108pro runtime sprites unchanged and subset OFL Korean fonts."""
from pathlib import Path
import ast
import hashlib
import json
import re
import shutil
import subprocess
import sys
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / '.tools/python'))
OUT = ROOT / 'native/generated'
REF = ROOT / 'references/f108pro'
PIN = 'ddab71ab58e353a0b9d1a60274ae9a1a05b0d965'

def pixel_font(glyphs, scale):
    height = 5*scale
    code = ['/* Numeric pixel shapes adapted under MIT; see F108pro-LICENSE.txt. */', '#include "lvgl.h"', '#include <string.h>']
    for char, rows in glyphs.items():
        w, h = len(rows[0])*scale, len(rows)*scale
        bits = [int(rows[y//scale][x//scale] == '1') for y in range(h) for x in range(w)]
        bits += [0]*((-len(bits))%8)
        packed = [sum(bits[i+j] << (7-j) for j in range(8)) for i in range(0,len(bits),8)]
        code.append(f'static const uint8_t glyph_{ord(char)}[] = {{'+','.join(str(v) for v in packed)+'};')
    code.extend(['static bool describe(const lv_font_t* f, lv_font_glyph_dsc_t* d, uint32_t c, uint32_t next) {',
                 f'(void)f; (void)next; memset(d,0,sizeof(*d)); d->bpp=1; d->box_h={height};', 'switch(c) {'])
    for char, rows in glyphs.items():
        code.append(f'case {ord(char)}: d->box_w={len(rows[0])*scale}; d->adv_w={len(rows[0])*scale+scale}; return true;')
    code.extend(['default: return false; }}', 'static const uint8_t* bitmap(const lv_font_t* f,uint32_t c) { (void)f; switch(c) {'])
    for char in glyphs: code.append(f'case {ord(char)}: return glyph_{ord(char)};')
    code.extend(['default: return NULL; }}', f'const lv_font_t font_pixel{height} = {{ .get_glyph_dsc=describe, .get_glyph_bitmap=bitmap, .line_height={height}, .base_line=0 }};'])
    (OUT / f'font_pixel{height}.c').write_text('\n'.join(code)+'\n', encoding='utf-8')

def generate():
    OUT.mkdir(parents=True, exist_ok=True)
    actual = subprocess.check_output(['git', '-C', str(REF), 'rev-parse', 'HEAD'], text=True).strip()
    if actual != PIN:
        raise RuntimeError('Reference revision differs from the reviewed source')
    shutil.copyfile(REF / 'LICENSE', OUT / 'F108pro-LICENSE.txt')
    shutil.copyfile(ROOT / '.tools/fonts/OFL.txt', OUT / 'NotoSansKR-OFL.txt')
    declarations = ['#pragma once', '#include "lvgl.h"', '#ifdef __cplusplus', 'extern "C" {', '#endif']
    manifest = {'source': 'https://github.com/guyster323/F108pro_Usage_monitor', 'commit': PIN,
                'license': 'MIT', 'format': 'LVGL 8 RGB565 little-endian + alpha', 'sprites': []}
    states = ['idle', 'caution', 'critical', 'exhausted', 'offline', 'stale']
    for provider in ['codex', 'cursor', 'claude', 'grok', 'copilot']:
        names = []
        for state in states:
            name = f'sprite_{provider}_{state}'
            path = ROOT / f'assets/providers/{provider}/{state}_00.png'
            im = Image.open(path).convert('RGBA')
            assert im.size == (88, 108)
            data = bytearray()
            for r, g, b, a in im.get_flattened_data():
                value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
                data.extend([value & 255, value >> 8, a])
            attribution = 'MIT Quotamon contributors; imagegen original asset.' if provider=='copilot' else f'MIT QuotaDeck contributors; source commit {PIN}. See F108pro-LICENSE.txt.'
            lines = [f'/* {attribution} */',
                     '#include "lvgl.h"', f'static const uint8_t {name}_map[] = {{']
            lines.extend(','.join(f'0x{v:02x}' for v in data[i:i+24])+',' for i in range(0, len(data), 24))
            lines.extend(['};', f'const lv_img_dsc_t {name} = {{',
                          '  .header = { .cf = LV_IMG_CF_TRUE_COLOR_ALPHA, .always_zero = 0, .reserved = 0, .w = 88, .h = 108 },',
                          f'  .data_size = {len(data)}, .data = {name}_map', '};'])
            (OUT / f'{name}.c').write_text('\n'.join(lines)+'\n', encoding='utf-8')
            declarations.append(f'extern const lv_img_dsc_t {name};')
            names.append('&'+name)
            manifest['sprites'].append({'provider': provider, 'state': state, 'source': str(path.relative_to(ROOT)).replace('\\','/'),
                                        'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'bytes': len(data)})
        declarations.append(f'extern const lv_img_dsc_t* const {provider}_sprites[6];')
        (OUT / f'{provider}_sprites.c').write_text('#include "assets.h"\n'+f'const lv_img_dsc_t* const {provider}_sprites[6] = {{'+','.join(names)+'};\n', encoding='utf-8')
    declarations.extend(['#ifdef __cplusplus', '}', '#endif'])
    (OUT / 'assets.h').write_text('\n'.join(declarations)+'\n', encoding='utf-8')
    # Read only a literal glyph dictionary; never import or run reference application code.
    parsed = ast.parse((REF / 'src/quotadeck/renderer/canvas.py').read_text(encoding='utf-8'))
    glyphs = next(ast.literal_eval(n.value) for n in parsed.body if isinstance(n, ast.Assign)
                  and any(isinstance(t, ast.Name) and t.id == 'PIXEL_GLYPHS' for t in n.targets))
    glyphs = {c: glyphs[c] for c in '0123456789%−-' if c in glyphs}
    pixel_font(glyphs, 5)
    pixel_font(glyphs, 8)
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    font = instantiateVariableFont(TTFont(ROOT / '.tools/fonts/NotoSansKR.ttf'), {'wght': 500}, inplace=True)
    static = ROOT / '.tools/fonts/NotoSansKR-Medium.ttf'
    font.save(static)
    # Explicit symbols used by native UI only; no account credentials or user data involved.
    text = ''.join(p.read_text(encoding='utf-8') for p in (ROOT / 'native').glob('*.cpp'))
    symbols = ''.join(sorted(set(re.findall(r'[^\x00-\x7f]', text))))
    binary = ROOT / '.tools/fontconv/node_modules/lv_font_conv/lv_font_conv.js'
    for size in [14,16,20]:
        subprocess.run(['node', str(binary), '--font', str(static), '--size', str(size), '--bpp', '4',
                        '--format', 'lvgl', '--range', '0x20-0x7e,0xa0-0x24f,0xac00-0xd7a3', '--symbols', symbols, '--no-compress',
                        '-o', str(OUT / f'font_ko{size}.c')], check=True, cwd=ROOT)
    template = (ROOT / 'third_party/lvgl/lv_conf_template.h').read_text(encoding='utf-8')
    template = template.replace('#if 0 /*Set it', '#if 1 /*Set it').replace('(48U * 1024U)', '(512U * 1024U)')
    template = template.replace('#define LV_FONT_FMT_TXT_LARGE 0', '#define LV_FONT_FMT_TXT_LARGE 1')
    template = template.replace('#define LV_USE_LOG 0', '#define LV_USE_LOG 1').replace('#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN', '#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN').replace('#define LV_LOG_PRINTF 0', '#define LV_LOG_PRINTF 1')
    (ROOT / 'native/lv_conf.h').write_text(template, encoding='utf-8')
    manifest['korean_font']={'name':'Noto Sans KR Medium (weight 500)', 'license':'SIL OFL 1.1',
                            'source':'https://github.com/google/fonts/tree/main/ofl/notosanskr',
                            'source_sha256': hashlib.sha256((ROOT / '.tools/fonts/NotoSansKR.ttf').read_bytes()).hexdigest(),
                            'sizes':[14,16,20], 'symbols':symbols, 'dynamic_account_name_glyphs':'All 11,172 modern Korean syllables, ASCII and Latin extended; unsupported scripts need additional font coverage'}
    manifest['copilot']='Original imagegen lavender pilot robot; assets/copilot/provenance.json'
    (OUT / 'asset-manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print('Generated 30 provider sprites, split-meter pixel fonts and Korean/Latin fonts.')

if __name__ == '__main__':
    generate()
