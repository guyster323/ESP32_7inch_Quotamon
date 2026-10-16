"""Exercise shared LVGL animations, export real-renderer GIFs and record evidence."""
from pathlib import Path
import json
import subprocess
import hashlib
import re
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
frames = ROOT / '.local/animation-frames'
out = ROOT / 'visuals/native'
manifest = json.loads((ROOT / 'native/generated/asset-manifest.json').read_text(encoding='utf-8'))
for sprite in manifest['sprites']:
    source = ROOT / sprite['source']
    assert hashlib.sha256(source.read_bytes()).hexdigest() == sprite['sha256'], source
    suffix = '_01' if sprite['frame'] else ''
    generated = ROOT / f"native/generated/sprite_{sprite['provider']}_{sprite['state']}{suffix}.c"
    initializer = generated.read_text(encoding='utf-8').split('_map[] = {', 1)[1].split('};', 1)[0]
    data = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', initializer))
    assert data[:4] == b'QPA1'
    count = int.from_bytes(data[4:6], 'little')
    assert 1 <= count <= 256 and len(data) == 8 + count * 3 + 88 * 108
    palette = [data[8+i*3:11+i*3] for i in range(count)]
    decoded = b''.join(palette[index] for index in data[8+count*3:])
    with Image.open(source) as image:
        expected = bytearray()
        for r, g, b, a in image.convert('RGBA').get_flattened_data():
            rgb = (r >> 3) << 11 | (g >> 2) << 5 | b >> 3
            expected.extend((rgb & 255, rgb >> 8, a))
    assert decoded == expected, f'Lossless RGB565 verification failed: {source}'
result = subprocess.run([str(ROOT / 'build/native/espmonitor_sim.exe'), '--render-animation', str(frames)],
                        cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
assert result.returncode == 0, f'Animation renderer exited {result.returncode}: {result.stdout}{result.stderr}'
assert '[Warn]' not in result.stdout + result.stderr, result.stdout + result.stderr
report = json.loads((frames / 'animation-checks.json').read_text(encoding='utf-8'))
report['lossless_palette_assets_checked'] = len(manifest['sprites'])


def export(pattern, filename, expected):
    sources = sorted(frames.glob(pattern))
    assert len(sources) == expected, (pattern, len(sources))
    images = []
    for source in sources:
        with Image.open(source) as image:
            assert image.size == (800, 480)
            images.append(image.convert('RGB').quantize(colors=128))
    images[0].save(out / filename, save_all=True, append_images=images[1:], duration=100,
                   loop=0, optimize=True, disposal=1)
    return {'path': str((out / filename).relative_to(ROOT)).replace('\\', '/'),
            'frames': expected, 'duration_ms': expected * 100}


report['previews'] = [export('home-*.ppm', 'character-animation.gif', 100),
                      export('detail-*.ppm', 'character-detail-animation.gif', 80),
                      export('state-*.ppm', 'character-states-animation.gif', 200)]
report['renderer'] = 'Shared LVGL 8.4 native UI, synthetic usage only'
(out / 'animation-checks.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
print(result.stdout.strip())
print('Exported home, detail and severity GIF previews from the actual LVGL framebuffer.')
