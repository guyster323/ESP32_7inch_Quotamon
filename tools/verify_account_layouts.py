"""Check selected-account layouts and pointer navigation in the shared LVGL UI."""
from pathlib import Path
import json
import subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
work=ROOT/'.local/account-counts'
out=ROOT/'visuals/account-counts';out.mkdir(parents=True,exist_ok=True)
result=subprocess.run([str(ROOT/'build/native/espmonitor_sim.exe'),'--render-account-counts',str(work)],
    capture_output=True,text=True,encoding='utf8',errors='replace',cwd=ROOT)
assert result.returncode==0,result.stdout+result.stderr
assert '[Warn]' not in result.stdout+result.stderr,result.stdout+result.stderr
checks=json.loads((work/'checks.json').read_text())
assert checks['passed'] and not checks['production_layout_changed']
for name in [f'home-{count}' for count in range(6)]+['home-6-last-page','home-20-last-page']:
    with Image.open(work/(name+'.ppm')) as picture:
        assert picture.size==(800,480)
        picture.save(out/(name+'.png'))
(out/'checks.json').write_text(json.dumps(checks,indent=2)+'\n')
print('0–5 selected, non-contiguous selections, pointer details, 6/20-account pagination: PASS; production layout unchanged.')
