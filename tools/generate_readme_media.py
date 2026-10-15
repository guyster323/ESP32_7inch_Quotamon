"""Build the README's illustrated motion demo from real LVGL renders and mock data.

Requires Pillow and build/native/espmonitor_sim.exe. Reads no running apps, accounts,
or device data. The bezel, highlights and transitions are presentation graphics.
"""
from pathlib import Path
import hashlib
import json
import subprocess
import time
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/readme'
WORK = ROOT / '.local/readme-media'
SIZE = (1200, 900)
MINT = '#70e4bf'
MUTED = '#91a6b8'
WHITE = '#eef7fb'
FRAME_MS = 80
STEPS = ['AT A GLANCE', 'TOUCH TO EXPLORE', 'KNOW THE RESET', 'STAY IN SYNC']


def font(size, bold=False):
    # Noto Sans KR is already used by the device; keep the same OFL font source.
    path = ROOT / '.tools/fonts/NotoSansKR-Medium.ttf'
    if not path.exists():
        path = Path('C:/Windows/Fonts/seguisb.ttf' if bold else 'C:/Windows/Fonts/segoeui.ttf')
    return ImageFont.truetype(str(path), size)


FONTS = {size: font(size, size >= 24) for size in [12, 14, 16, 18, 22, 26, 52]}


def text(draw, xy, value, size=18, fill=WHITE, anchor=None):
    draw.text(xy, value, font=FONTS[size], fill=fill, anchor=anchor, stroke_width=0)


def ease(value):
    value = max(0, min(1, value))
    return value * value * (3 - 2 * value)


def fixture(remaining=72, refreshed='14:33:00'):
    accounts = []
    examples = [
        ('codex', 'Orca', 'Business Premium', ['5H', 'WEEKLY'], [remaining, 54], ['2h 18m', '3d 4h']),
        ('codex', 'Personal', 'Plus', ['5H', 'WEEKLY'], [38, 18], ['1h 40m', '2d 8h']),
        ('codex', 'Team', 'Business standard', ['5H', 'WEEKLY'], [9, 61], ['46m', '4d 2h']),
        ('cursor', 'Ultra', 'Ultra', ['Cursor Models', 'Other Models'], [82, 47], ['17d 0h', '17d 0h']),
        ('copilot', 'Business', 'Business', ['MONTHLY'], [100], ['16d 0h'])
    ]
    for i, (provider, profile, plan, names, values, resets) in enumerate(examples):
        accounts.append(dict(id=f'public-demo-{i}', provider=provider,
            title=f'{provider.capitalize()} / {profile}', profile=profile, plan=plan,
            enabled=True, state='ready', updated='14:32:00', source_note='',
            has_amounts=False, has_session=False,
            pools=[dict(name=n, remaining=v, reset=r) for n, v, r in zip(names, values, resets)]))
    # Match the existing native demo's labels without using any user's account IDs.
    accounts[0]['profile'] = 'Orca 시스템 기본값'
    accounts[3]['profile'] = '개인 계정'
    accounts[4]['profile'] = '회사 제공 계정'
    return dict(protocol='quotamon.v1', produced_at=time.time(), refreshed=refreshed,
        config_revision=0, connected=True, mock=True, accounts=accounts,
        display=dict(poll_seconds=60, brightness_percent=70, show_reset=True,
            show_updated=True, show_credits=True, show_session=True))


def native_render(name, remaining=72, refreshed='14:33:00'):
    folder = WORK / name
    folder.mkdir(parents=True, exist_ok=True)
    source = folder / 'snapshot.json'
    source.write_text(json.dumps(fixture(remaining, refreshed), ensure_ascii=False), encoding='utf-8')
    result = subprocess.run([str(ROOT / 'build/native/espmonitor_sim.exe'),
        '--render-snapshot', str(source), str(folder)], cwd=ROOT,
        capture_output=True, text=True, encoding='utf-8', errors='replace', check=True)
    if '[Warn]' in result.stdout + result.stderr:
        raise RuntimeError('The native renderer reported a warning')
    return Image.open(folder / '14-source-snapshot.ppm').convert('RGB')


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
    text(draw, (64, 34), 'Q U O T A M O N   /   Y O U R   D E S K .   I N   S Y N C .', 14, MINT)
    text(draw, (61, 67), 'Your quotas. In view.', 52)
    text(draw, (65, 143), 'Codex × 3   /   Cursor   /   Copilot', 18, MUTED)
    draw.rounded_rectangle((970, 43, 1136, 75), radius=16, fill='#142820', outline='#285444')
    draw.ellipse((985, 55, 992, 62), fill=MINT)
    text(draw, (1003, 47), '7-inch · 800 × 480', 14, MINT)
    text(draw, (65, 864), 'ILLUSTRATIVE DATA  ·  ACTUAL LVGL UI  ·  ILLUSTRATED DEVICE', 12, MUTED)
    text(draw, (1135, 864), 'ESP32-S3  /  BLE', 12, MUTED, 'ra')
    return image


BACKGROUND = None


def compose(screen, chapter, t, highlights=(), tap=None):
    image = BACKGROUND.copy()
    draw = ImageDraw.Draw(image)
    for i, label in enumerate(STEPS):
        x = 64 + i * 271
        active = i == chapter
        draw.rounded_rectangle((x, 193, x + 259, 231), radius=10,
            fill='#173a31' if active else '#111e2a', outline='#3c8e74' if active else '#263543')
        text(draw, (x + 14, 200), f'{i + 1:02}  {label}', 14, MINT if active else MUTED)

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
    captions = [
        ('Five accounts. One quiet glance.', 'The same three Codex cards, Cursor and Copilot as the desk display.'),
        ('Touch a card. See the whole picture.', 'Native detail pages keep each quota window separate.'),
        ('Know what resets. Know when.', '5-hour + weekly schedules. One shared reset for Cursor.'),
        ('A fresh screen. An honest timestamp.', 'Display refresh and provider collection time stay separate.')
    ]
    title, subtitle = captions[chapter]
    text(draw, (600, 807), title, 26, WHITE, 'mt')
    text(draw, (600, 846), subtitle, 14, MUTED, 'mt')
    # A thin progress rail makes the looping storyboard easy to follow.
    draw.rounded_rectangle((64, 886, 1136, 889), radius=2, fill='#22313e')
    draw.rounded_rectangle((64, 886, 65 + int(1070 * t / 14.4), 889), radius=2, fill=MINT)
    return image.convert('RGB')


def main():
    global BACKGROUND
    OUT.mkdir(parents=True, exist_ok=True)
    WORK.mkdir(parents=True, exist_ok=True)
    native = ROOT / 'build/native/espmonitor_sim.exe'
    if not native.exists():
        raise SystemExit('Build the native simulator first: tools\\build_native.cmd')
    home = native_render('home')
    detail = Image.open(ROOT / 'visuals/native/02-codex-detail.png').convert('RGB')
    updates = [native_render(f'refresh-{i:02}', 72 - i / 3, '14:34:00') for i in range(13)]
    BACKGROUND = background()
    poster = compose(home, 0, 0)
    poster.save(OUT / 'quotamon-display.png', optimize=True)
    frames = []
    for index in range(180):
        t = index * FRAME_MS / 1000
        if t < 3.2:
            chapter, screen = 0, home
            card = min(2, int(t / 1.1))
            highlights = [((19 + card*257, 71, 265 + card*257, 242), MINT)] if t > .55 else []
        elif t < 6.2:
            chapter, screen, highlights = 1, detail, []
            screen = Image.blend(home, detail, ease((t - 3.2) / .4))
        elif t < 9.8:
            chapter, highlights = 2, [((28, 217, 258, 239), MINT), ((28, 398, 385, 430), '#65bbff')]
            screen = Image.blend(detail, home, ease((t - 6.2) / .4))
        else:
            chapter, highlights = 3, [((24, 455, 482, 478), MINT)]
            step = round(ease((t - 10.2) / 1.3) * 12)
            screen = updates[step]
            if t > 13.5:
                screen = Image.blend(screen, home, ease((t - 13.5) / .7))
                highlights = []
        tap = (160, 169, (t - 2.64) / .56) if 2.64 <= t < 3.2 else None
        frames.append(compose(screen, chapter, t, highlights, tap))

    # One shared palette avoids color flicker and keeps GitHub's GIF download small.
    swatches = Image.new('RGB', (1200, 900 * 4))
    for i, sample in enumerate([frames[0], frames[45], frames[90], frames[145]]):
        swatches.paste(sample, (0, i * 900))
    palette = swatches.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
    indexed = [frame.quantize(palette=palette, dither=Image.Dither.NONE) for frame in frames]
    target = OUT / 'quotamon-demo.gif'
    indexed[0].save(target, save_all=True, append_images=indexed[1:], duration=FRAME_MS,
        loop=0, optimize=True, disposal=1)
    with Image.open(target) as gif:
        durations = []
        for i in range(gif.n_frames):
            gif.seek(i)
            durations.append(gif.info['duration'])
        assert gif.size == SIZE and gif.info['loop'] == 0
        assert sum(durations) == 14400
    assert target.stat().st_size < 10_000_000, 'README GIF is too large'
    metadata = dict(size=list(SIZE), duration_ms=14400, loop=True,
        native_screen_size=[800, 480], illustrative_data=True, live_account_data_used=False,
        native_ui_source='native/ui.cpp', detail_source='visuals/native/02-codex-detail.png',
        presentation_only=['device bezel', 'touch indicator', 'highlights', 'transitions'],
        regeneration='python tools/generate_readme_media.py',
        files={p.name:dict(bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest())
            for p in [target, OUT / 'quotamon-display.png']})
    (OUT / 'media.json').write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8')
    # Contact sheet for inspecting every chapter without a browser or a live device.
    sheet = Image.new('RGB', (1200, 900))
    for i, number in enumerate([20, 52, 100, 145]):
        sheet.paste(frames[number].resize((600, 450), Image.Resampling.LANCZOS), ((i % 2) * 600, (i // 2) * 450))
    sheet.save(WORK / 'contact-sheet.png')
    print(f'Motion demo: {target.stat().st_size:,} bytes / 14.4 seconds / 1200×900')
    print('Poster, looping GIF and provenance metadata generated from mock native UI.')


def export_video():
    """Optional H.264 MP4 export; requires imageio-ffmpeg."""
    import imageio_ffmpeg

    executable = Path(imageio_ffmpeg.get_ffmpeg_exe())
    if not executable.exists():
        raise RuntimeError('The FFmpeg executable is missing')
    target = OUT / 'quotamon-demo.mp4'
    subprocess.run([str(executable), '-y', '-i', str(OUT / 'quotamon-demo.gif'),
        '-an', '-c:v', 'libx264', '-crf', '19', '-pix_fmt', 'yuv420p',
        '-movflags', '+faststart', str(target)], capture_output=True, check=True)
    metadata_file = OUT / 'media.json'
    metadata = json.loads(metadata_file.read_text(encoding='utf-8'))
    metadata['files'][target.name] = dict(bytes=target.stat().st_size,
        sha256=hashlib.sha256(target.read_bytes()).hexdigest())
    metadata['video_mime'] = 'video/mp4;codecs=avc1'
    metadata['video_source'] = 'quotamon-demo.gif'
    metadata_file.write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
    print(f'MP4 exported: {target.stat().st_size:,} bytes; H.264')


if __name__ == '__main__':
    if '--video-only' not in sys.argv:
        main()
    if '--video' in sys.argv or '--video-only' in sys.argv:
        export_video()
