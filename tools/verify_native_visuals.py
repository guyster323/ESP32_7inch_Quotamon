"""Run the real LVGL renderer and validate native screenshots and font coverage."""
from pathlib import Path
from collections import Counter
import json
import subprocess
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
folder = ROOT / 'visuals/native'
result = subprocess.run([str(ROOT / 'build/native/espmonitor_sim.exe'), '--render', str(folder)],
                        cwd=ROOT, capture_output=True, text=True, encoding='utf-8', errors='replace')
assert result.returncode == 0, result.stdout + result.stderr
assert 'glyph dsc. not found' not in result.stdout + result.stderr, result.stdout + result.stderr
assert '[Warn]' not in result.stdout + result.stderr, result.stdout + result.stderr
renders=sorted(p for p in folder.glob('*.ppm') if p.stem!='14-source-snapshot')
for ppm in renders:
    image = Image.open(ppm)
    assert image.size == (800,480)
    image.save(ppm.with_suffix('.png'))
home = Image.open(folder / '01-home.png').convert('RGB')
# The first 72% meter deliberately intersects the % text: both text inks must exist.
inks = list(home.crop((136,130,251,154)).get_flattened_data())
assert any(min(c)>=240 for c in inks), 'Filled meter lacks white pixel text'
assert any(max(c)<=25 for c in inks), 'Empty meter lacks dark pixel text'
offline = Image.open(folder / '06-disconnected.png').convert('RGB')
for rectangle in [(134,113,250,153),(391,113,507,153),(648,113,765,153),
                  (134,295,380,330),(520,295,765,330)]:
    colors = list(offline.crop(rectangle).get_flattened_data())
    assert not any(g>130 and g>r*1.7 and g>b*1.3 for r,g,b in colors), 'Offline meter retains green quota fill'
    assert not any(b>160 and b>r*1.4 for r,g,b in colors), 'Offline meter retains blue quota fill'
checks = json.loads((folder / 'native-checks.json').read_text())
checks['visual_checks']=[f'{len(renders)} native screenshots at 800x480','embedded Korean glyph coverage without LVGL warnings',
                         'same percentage text rendered white on fill and dark on empty',
                         'no quota fills after disconnect']
checks['lvgl_version']='8.4.0'
checks['lvgl_commit']='4495f428630cc1741bd8bfd977f080e8460e8e8d'
checks['reference_commit']='ddab71ab58e353a0b9d1a60274ae9a1a05b0d965'
(folder / 'native-checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
print(result.stdout.strip())
print('PNG output, glyph coverage, split text inks and disconnected meters verified.')
