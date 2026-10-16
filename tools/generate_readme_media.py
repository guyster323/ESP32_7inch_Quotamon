"""Build the README's illustrated motion demo from real LVGL renders and mock data.

Requires Pillow, imageio-ffmpeg and build/native/espmonitor_sim.exe. Reads no running apps, accounts,
or device data. The bezel, highlights and transitions are presentation graphics.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import argparse
import gc
import sys
import shutil
import urllib.request

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/readme'
WORK = ROOT / '.local/readme-media'
SIZE = (1200, 900)
MINT = '#70e4bf'
MUTED = '#91a6b8'
WHITE = '#eef7fb'
FRAME_MS = 100
FRAME_COUNT = 240
LOCALES = json.loads((ROOT / 'tools/readme_media_locales.json').read_text(encoding='utf-8'))
LANGUAGE = 'ko'


def font(size, bold=False):
    # Noto Sans KR is already used by the device; keep the same OFL font source.
    name='NotoSansSC-Medium.ttf' if LANGUAGE=='zh-CN' else 'NotoSansJP-Medium.ttf' if LANGUAGE=='ja' else 'NotoSansKR-Medium.ttf'
    path = ROOT / '.tools/fonts' / name
    if not path.exists():
        raise RuntimeError(f'Missing reviewed OFL font: {path}')
    return ImageFont.truetype(str(path), size)


FONTS = {}


def text(draw, xy, value, size=18, fill=WHITE, anchor=None):
    width = draw.textlength(value, font=FONTS[size])
    assert width <= 1072, (LANGUAGE, value, width)
    draw.text(xy, value, font=FONTS[size], fill=fill, anchor=anchor, stroke_width=0)


def ease(value):
    value = max(0, min(1, value))
    return value * value * (3 - 2 * value)


def native_render(language):
    folder = WORK / language
    folder.mkdir(parents=True, exist_ok=True)
    result = subprocess.run([str(ROOT / 'build/native/espmonitor_sim.exe'),
        '--render-readme', language, str(folder)], cwd=ROOT,
        capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)
    if '[Warn]' in result.stdout + result.stderr:
        raise RuntimeError('The native renderer reported a warning')
    checks = json.loads((folder / 'checks.json').read_text())
    assert checks['passed'] and checks['language'] == language
    return folder, checks


def background():
    image = Image.new('RGB', SIZE, '#080e18')
    glow = Image.new('RGB', SIZE)
    draw = ImageDraw.Draw(glow)
    draw.ellipse((670, -180, 1400, 500), fill='#12382f')
    draw.ellipse((-400, 430, 420, 1180), fill='#221c36')
    glow = glow.filter(ImageFilter.GaussianBlur(115))
    image = Image.blend(image, glow, .38)
    draw = ImageDraw.Draw(image)
    for x in range(0, SIZE[0], 40):
        for y in range(0, SIZE[1], 40):
            draw.point((x, y), fill='#1b2934')
    copy = LOCALES[LANGUAGE]
    text(draw, (64, 34), 'Q U O T A M O N   /   ' + copy['name'], 14, MINT)
    text(draw, (61, 67), copy['headline'], 52)
    text(draw, (65, 143), 'Codex × 3   /   Cursor   /   Copilot', 18, MUTED)
    draw.rounded_rectangle((970, 43, 1136, 75), radius=16, fill='#142820', outline='#285444')
    draw.ellipse((985, 55, 992, 62), fill=MINT)
    text(draw, (1003, 47), '7-inch · 800 × 480', 14, MINT)
    text(draw, (65, 864), copy['disclaimer'], 12, MUTED)
    text(draw, (1135, 864), 'ESP32-S3  /  BLE', 12, MUTED, 'ra')
    return image


BACKGROUND = None


def compose(screen, chapter, t, highlights=(), tap=None):
    image = BACKGROUND.copy()
    draw = ImageDraw.Draw(image)
    for i, label in enumerate(LOCALES[LANGUAGE]['steps']):
        x = 64 + i * 217
        active = i == chapter
        draw.rounded_rectangle((x, 193, x + 204, 231), radius=10,
            fill='#173a31' if active else '#111e2a', outline='#3c8e74' if active else '#263543')
        value=f'{i + 1:02}  {label}'
        size=14 if draw.textlength(value,font=FONTS[14])<=186 else 12
        assert draw.textlength(value,font=FONTS[size])<=186,(LANGUAGE,value)
        text(draw, (x + 9, 200), value, size, MINT if active else MUTED)

    # The display pixels are pasted at their native 800×480 resolution.
    shadow = Image.new('RGBA', SIZE)
    ImageDraw.Draw(shadow).rounded_rectangle((170, 271, 1030, 801), radius=35, fill=(0, 0, 0, 160))
    image = Image.alpha_composite(image.convert('RGBA'), shadow.filter(ImageFilter.GaussianBlur(20)))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle((173, 249, 1027, 781), radius=28, fill='#0d1721', outline='#344852', width=2)
    draw.rounded_rectangle((181, 257, 1019, 773), radius=22, fill='#0a1018', outline='#1a2a35')
    image.paste(screen, (200, 276))
    draw = ImageDraw.Draw(image)
    text(draw, (193, 758), 'QUOTAMON', 12, '#69848f')
    draw.ellipse((999, 764, 1004, 769), fill=MINT)
    for bounds, color in highlights:
        x1, y1, x2, y2 = bounds
        draw.rounded_rectangle((200 + x1, 276 + y1, 200 + x2, 276 + y2), radius=5,
            outline=color, width=2)
    if tap:
        x, y, progress = tap
        r = 7 + int(progress * 17)
        ring = Image.new('RGBA', SIZE)
        ImageDraw.Draw(ring).ellipse((200+x-r, 276+y-r, 200+x+r, 276+y+r),
            outline=(112, 228, 191, int(240 * (1-progress))), width=3)
        image = Image.alpha_composite(image, ring)

    draw = ImageDraw.Draw(image)
    title, subtitle = LOCALES[LANGUAGE]['captions'][chapter]
    text(draw, (600, 807), title, 26, WHITE, 'mt')
    text(draw, (600, 846), subtitle, 14, MUTED, 'mt')
    # A thin progress rail makes the looping storyboard easy to follow.
    draw.rounded_rectangle((64, 886, 1136, 889), radius=2, fill='#22313e')
    draw.rounded_rectangle((64, 886, 65 + int(1070 * t / 24), 889), radius=2, fill=MINT)
    return image.convert('RGB')


def main(languages):
    global BACKGROUND, LANGUAGE, FONTS
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    native = ROOT / 'build/native/espmonitor_sim.exe'
    if not native.exists():
        raise SystemExit('Build the native simulator first: tools\\build_native.cmd')
    sys.path.insert(0,str(ROOT/'.tools/python'))
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    pin='2894aab31764f10f29c421bdfd2340d3b382d384'
    jp=ROOT/'.tools/fonts/NotoSansJP.ttf'
    jp_expected='c2f3b4d463500a2ddcd3849cded1fceeb9fd6d1c32e6cbecd568453ba50fc68f'
    if not jp.exists():
        with urllib.request.urlopen(f'https://raw.githubusercontent.com/google/fonts/{pin}/ofl/notosansjp/NotoSansJP%5Bwght%5D.ttf',timeout=60) as response:jp.write_bytes(response.read())
    assert hashlib.sha256(jp.read_bytes()).hexdigest()==jp_expected,'Unreviewed Japanese caption font'
    jp_static=jp.with_name('NotoSansJP-Medium.ttf')
    if not jp_static.exists():instantiateVariableFont(TTFont(jp),{'wght':500},inplace=True).save(jp_static)
    jp_license=jp.with_name('NotoSansJP-OFL.txt')
    if not jp_license.exists():
        with urllib.request.urlopen(f'https://raw.githubusercontent.com/google/fonts/{pin}/ofl/notosansjp/OFL.txt',timeout=60) as response:jp_license.write_bytes(response.read())
    shutil.copyfile(jp_license,OUT/'NotoSansJP-OFL.txt')
    for code in languages:
        filename='NotoSansSC-Medium.ttf' if code=='zh-CN' else 'NotoSansJP-Medium.ttf' if code=='ja' else 'NotoSansKR-Medium.ttf'
        available=TTFont(ROOT/'.tools/fonts'/filename).getBestCmap()
        values=json.dumps(LOCALES[code],ensure_ascii=False)
        missing=sorted({c for c in values if ord(c)>127 and ord(c) not in available})
        assert not missing,(code,'Missing caption glyphs',missing)
    metadata = dict(size=list(SIZE), duration_ms=24000, loop=True,
        native_screen_size=[800, 480], illustrative_data=True, live_account_data_used=False,
        native_ui_source='native/ui.cpp', native_renderer='native/simulator_win32.cpp --render-readme',
        presentation_only=['device bezel', 'captions', 'touch indicator', 'progress rail', 'screen-off and touch-wake simulation'],
        regeneration='python tools/generate_readme_media.py', languages={}, files={})
    metadata['japanese_caption_font']={'name':'Noto Sans JP Medium','source':f'https://github.com/google/fonts/tree/{pin}/ofl/notosansjp',
        'variable_font_sha256':jp_expected,'license':'NotoSansJP-OFL.txt'}
    taps={50:(150,165),80:(730,35),90:(655,135),110:(665,185),120:(65,118),130:(440,176),
          140:(400,140),146:(520,140),152:(520,140),158:(400,338),166:(520,338),180:(735,432),220:(400,240)}
    for code in languages:
        print(f'Rendering {code}: native UI, animation and touch sequence...',flush=True)
        LANGUAGE=code
        FONTS={size:font(size) for size in [12,14,16,18,22,26,52]}
        folder,checks=native_render(code)
        target_folder=OUT if code=='ko' else OUT/code
        target_folder.mkdir(exist_ok=True)
        BACKGROUND=background();frames=[]
        for index in range(FRAME_COUNT):
            chapter=0 if index<50 else 1 if index<80 else 2 if index<110 else 3 if index<180 else 4
            active=[(n,xy) for n,xy in taps.items() if 0<=index-n<4]
            tap=(*active[-1][1],(index-active[-1][0])/4) if active else None
            with Image.open(folder/f'frame-{index:03}.ppm') as screen:
                frames.append(compose(screen,chapter,index/10,tap=tap))
        frames[0].save(target_folder/'quotamon-display.png',optimize=True)
        swatches=Image.new('RGB',(1200,900*6))
        for i,n in enumerate([0,55,95,145,175,210]):swatches.paste(frames[n],(0,i*900))
        palette=swatches.quantize(colors=256,method=Image.Quantize.MEDIANCUT)
        indexed=[frame.quantize(palette=palette,dither=Image.Dither.NONE) for frame in frames]
        target=target_folder/'quotamon-demo.gif'
        indexed[0].save(target,save_all=True,append_images=indexed[1:],duration=FRAME_MS,loop=0,optimize=True,disposal=1)
        assert target.stat().st_size<10_000_000,'README GIF is too large'
        export_video(target_folder)
        metadata['languages'][code]={'name':LOCALES[code]['name'],'base_path':'' if code=='ko' else code+'/',
            'native_checks':checks,'native_frames_sha256':hashlib.sha256(b''.join(hashlib.sha256((folder/f'frame-{n:03}.ppm').read_bytes()).digest() for n in range(FRAME_COUNT))).hexdigest()}
        for path in target_folder.glob('quotamon-*'):
            metadata['files'][path.relative_to(OUT).as_posix()]={'bytes':path.stat().st_size,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
        sheet=Image.new('RGB',(1200,900))
        for i,n in enumerate([20,95,150,210]):sheet.paste(frames[n].resize((600,450),Image.Resampling.LANCZOS),((i%2)*600,(i//2)*450))
        sheet.save(WORK/f'contact-{code}.png')
        print(f'{code}: 24s / 240 animated native frames / GIF {target.stat().st_size:,} bytes + H.264 MP4',flush=True)
        del frames,indexed,swatches,palette,sheet
        gc.collect()
    metadata['video_mime']='video/mp4;codecs=avc1'
    metadata['captions_source']='tools/readme_media_locales.json'
    if set(languages)==set(LOCALES):
        (OUT / 'media.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
        template=(ROOT/'tools/readme_player.html').read_text(encoding='utf8')
        (OUT/'index.html').write_text(template.replace('__DEMO_LOCALES__',json.dumps(LOCALES,ensure_ascii=False)),encoding='utf8')


def export_video(folder):
    """H.264 MP4 with browser-compatible pixel format and fast-start metadata."""
    import imageio_ffmpeg

    executable = Path(imageio_ffmpeg.get_ffmpeg_exe())
    if not executable.exists():
        raise RuntimeError('The FFmpeg executable is missing')
    target = folder / 'quotamon-demo.mp4'
    subprocess.run([str(executable), '-y', '-i', str(folder / 'quotamon-demo.gif'),
        '-an', '-c:v', 'libx264', '-crf', '19', '-pix_fmt', 'yuv420p',
        '-movflags', '+faststart', str(target)], capture_output=True, check=True)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--language',choices=list(LOCALES),action='append',help='Preview a subset; default regenerates all five languages')
    args=parser.parse_args()
    main(args.language or list(LOCALES))
